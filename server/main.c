/*
 * OpenChess online server (Phase 3: resilience).
 *
 * A single-process libwebsockets server that lets clients:
 *   - create a private room and share its 6-character code,
 *   - join a room by code,
 *   - enter a public matchmaking queue,
 * and referees the games. On top of Phase 2's authoritative play it adds:
 *   - seat tokens + reconnect (a briefly dropped player reclaims their seat and
 *     receives the authoritative `state`),
 *   - a presence grace period (opponent notified; abandonment after a timeout),
 *   - heartbeats (server ping / client pong, and an rx watchdog), and
 *   - rematch (both players agree; colours swap, clocks reset).
 *
 * Usage: openchessd [port]      (default port 7681, or $OPENCHESS_PORT)
 *        OPENCHESS_GRACE_MS=...  override the disconnect grace period
 */

#include "libwebsockets.h"
#include "accounts.h"
#include "../src/proto.h"
#include "../src/board.h"
#include "../src/move.h"
#include "../src/fen.h"
#include "../src/pgn.h"
#include <math.h>

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_SESS 256
#define MAX_ROOMS 256
#define SRV_QLEN 32
#define SRV_LINELEN 2048
#define CODE_LEN 6
#define MAX_PLY 1024

#define PING_MS      10000
#define RX_TIMEOUT_MS 30000
#define DEFAULT_GRACE_MS 30000

typedef struct {
    int idx;                     /* session slot for this connection */
} PerConn;

typedef struct {
    int          used;
    struct lws  *wsi;
    char         token[40];
    char         nick[40];
    int          room;           /* room index, or -1 */
    int          seat;           /* 0 = White, 1 = Black, or -1 */
    int          queued;
    int          want_time;
    int          want_inc;
    int          want_rated;
    /* account (when logged in) */
    int          user_id;
    bool         logged_in;
    char         user[40];
    char         auth_token[80];
    int          pvp_rating;
    int          puzzle_rating;
    uint64_t     last_rx;        /* for the heartbeat watchdog */
    uint64_t     last_ping;
    uint64_t     chat_win;       /* chat rate-limit window */
    int          chat_count;
    char         outq[SRV_QLEN][SRV_LINELEN];
    int          qh, qt, qn;
} Session;

typedef struct {
    int      used;
    char     code[CODE_LEN + 1];
    int      seat[2];                /* session slots, or -1 */
    int      started;
    int      over;
    int      rated;                  /* rated game (both logged in) */
    char     name[2][40];            /* player nicks, for spectators */
    char     seattok[2][40];         /* stable per-seat resume token */
    int      connected[2];
    uint64_t dc_ts[2];               /* when the seat disconnected */
    int      rematch[2];             /* rematch votes */
    Board    board;
    Board    pos[MAX_PLY + 1];        /* position at each ply (for repetition) */
    int      npos;
    Move     history[MAX_PLY];
    char     san[MAX_PLY][8];
    int      ply;
    int      init_time;              /* base clock ms, 0 = untimed */
    int      clock_ms[2];
    int      inc_ms;
    uint64_t last_ts;
} Room;

static Session sessions[MAX_SESS];
static Room    rooms[MAX_ROOMS];
static int     queue_slots[MAX_SESS];
static int     queue_count;
static int     g_grace_ms = DEFAULT_GRACE_MS;
static Accounts *g_accounts = NULL;

static const char CODE_ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static int  srv_cb(struct lws *wsi, enum lws_callback_reasons reason,
                   void *user, void *in, size_t len);

static const struct lws_protocols SRV_PROTOCOLS[] = {
    { .name = "chess", .callback = srv_cb, .per_session_data_size = sizeof(PerConn),
      .rx_buffer_size = 2048 },
    { 0 }
};

/* ---- helpers ------------------------------------------------------------ */

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int sidx(const Session *s) { return (int)(s - sessions); }

static void rand_token(char *out, size_t n)
{
    snprintf(out, n, "%08x%08x%08x", (unsigned)rand(), (unsigned)rand(),
             (unsigned)rand());
}

static void sess_raw_send(Session *s, const char *json)
{
    if (!s || !s->wsi || !json) return;
    if (s->qn >= SRV_QLEN) return;
    snprintf(s->outq[s->qt], SRV_LINELEN, "%s", json);
    s->qt = (s->qt + 1) % SRV_QLEN;
    s->qn++;
    lws_callback_on_writable(s->wsi);
}

static void sess_send(cJSON *msg, Session *s)
{
    char *text = proto_serialize(msg);   /* frees msg */
    if (!text) return;
    sess_raw_send(s, text);
    free(text);
}

static void send_welcome(Session *s)
{
    cJSON *m = proto_new(PROTO_S2C_WELCOME);
    cJSON_AddStringToObject(m, "token", s->token);
    sess_send(m, s);
}

static void add_clocks(cJSON *m, const Room *r)
{
    if (r->clock_ms[0] >= 0) {
        cJSON_AddNumberToObject(m, "wtime", r->clock_ms[0]);
        cJSON_AddNumberToObject(m, "btime", r->clock_ms[1]);
    }
}

static void send_state(Room *r, Session *s)
{
    if (!r || !s) return;
    char fen[128];
    fen_generate(&r->board, fen, sizeof fen);
    cJSON *m = proto_new(PROTO_S2C_STATE);
    cJSON_AddStringToObject(m, "fen", fen);
    cJSON_AddNumberToObject(m, "ply", r->ply);
    cJSON_AddStringToObject(m, "side", r->board.side == WHITE ? "white" : "black");
    cJSON_AddBoolToObject(m, "over", r->over != 0);
    add_clocks(m, r);
    sess_send(m, s);
}

static void send_reject(Session *s, const char *reason)
{
    cJSON *m = proto_new(PROTO_S2C_REJECT);
    cJSON_AddStringToObject(m, "reason", reason ? reason : "rejected");
    sess_send(m, s);
}

static void send_auth(Session *s, bool ok, const char *reason)
{
    cJSON *m = proto_new(PROTO_S2C_AUTH);
    cJSON_AddBoolToObject(m, "ok", ok);
    if (!ok) {
        cJSON_AddStringToObject(m, "reason", reason ? reason : "failed");
    } else {
        cJSON_AddStringToObject(m, "user", s->user);
        cJSON_AddStringToObject(m, "token", s->auth_token);
        cJSON_AddNumberToObject(m, "pvp_rating", s->pvp_rating);
        cJSON_AddNumberToObject(m, "puzzle_rating", s->puzzle_rating);
    }
    sess_send(m, s);
}

static void send_room(Session *s, Room *r)
{
    if (!s || !r) return;
    cJSON *m = proto_new(PROTO_S2C_ROOM);
    cJSON_AddStringToObject(m, "code", r->code);
    cJSON_AddStringToObject(m, "color", s->seat == 0 ? "white" : "black");
    add_clocks(m, r);
    sess_send(m, s);
}

static void send_start(Room *r)
{
    if (!r) return;
    Session *w = &sessions[r->seat[0]];
    Session *b = &sessions[r->seat[1]];
    char fen[128];
    fen_generate(&r->board, fen, sizeof fen);

    const char *colors[2] = { "white", "black" };
    Session *seats[2] = { w, b };
    Session *opps[2]  = { b, w };
    for (int i = 0; i < 2; i++) {
        cJSON *m = proto_new(PROTO_S2C_START);
        cJSON_AddStringToObject(m, "color", colors[i]);
        cJSON_AddStringToObject(m, "opponent", opps[i]->nick);
        cJSON_AddStringToObject(m, "code", r->code);
        cJSON_AddStringToObject(m, "fen", fen);
        add_clocks(m, r);
        sess_send(m, seats[i]);
    }
}

/* Free a room and detach both seats' sessions. */
static void detach_room(int ri)
{
    if (ri < 0 || ri >= MAX_ROOMS) return;
    Room *r = &rooms[ri];
    for (int sl = 0; sl < MAX_SESS; sl++) {
        if (sessions[sl].used && sessions[sl].room == ri) {
            sessions[sl].room = -1;
            sessions[sl].seat = -1;
        }
    }
    memset(r, 0, sizeof *r);
    r->seat[0] = r->seat[1] = -1;
}

static int find_room(const char *code)
{
    if (!code) return -1;
    for (int i = 0; i < MAX_ROOMS; i++)
        if (rooms[i].used && strcmp(rooms[i].code, code) == 0) return i;
    return -1;
}

static void gen_code(char out[CODE_LEN + 1])
{
    for (;;) {
        for (int i = 0; i < CODE_LEN; i++)
            out[i] = CODE_ALPHABET[rand() % (int)(sizeof CODE_ALPHABET - 1)];
        out[CODE_LEN] = '\0';
        if (find_room(out) < 0) return;
    }
}

static int alloc_room(char out_code[CODE_LEN + 1], int time_ms, int inc_ms)
{
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].used) {
            char code[CODE_LEN + 1];
            gen_code(code);
            memset(&rooms[i], 0, sizeof rooms[i]);
            rooms[i].used = 1;
            rooms[i].seat[0] = rooms[i].seat[1] = -1;
            snprintf(rooms[i].code, sizeof rooms[i].code, "%s", code);
            board_reset(&rooms[i].board);
            rooms[i].ply = 0;
            rooms[i].pos[0] = rooms[i].board;
            rooms[i].npos = 1;
            rooms[i].init_time = time_ms > 0 ? time_ms : 0;
            rooms[i].clock_ms[0] = rooms[i].clock_ms[1] =
                time_ms > 0 ? time_ms : -1;
            rooms[i].inc_ms = inc_ms > 0 ? inc_ms : 0;
            snprintf(out_code, CODE_LEN + 1, "%s", code);
            return i;
        }
    }
    return -1;
}

static void queue_remove(int slot)
{
    for (int i = 0; i < queue_count; i++) {
        if (queue_slots[i] == slot) {
            memmove(&queue_slots[i], &queue_slots[i + 1],
                    sizeof queue_slots[0] * (queue_count - i - 1));
            queue_count--;
            return;
        }
    }
}

static void start_match(Room *r)
{
    r->started = 1;
    r->over = 0;
    r->last_ts = now_ms();
    int a = r->seat[0], b = r->seat[1];
    r->rated = (a >= 0 && b >= 0 && sessions[a].logged_in && sessions[b].logged_in &&
                sessions[a].want_rated && sessions[b].want_rated);
    send_start(r);
}

/* Detach a session from a finished room so it can start something new. */
static void leave_if_over(Session *s)
{
    if (s->room < 0) return;
    Room *r = &rooms[s->room];
    if (!r->used || r->over) detach_room(s->room);
}

/* ---- rules -------------------------------------------------------------- */

static GameState referee_state(const Room *r)
{
    GameState gs = game_state(&r->board);
    if (gs != NO_GAME_OVER) return gs;
    if (r->board.halfmove_clock >= 100) return FIFTY_MOVE_RULE;
    if (board_repetitions(&r->board, r->pos, r->npos) >= 3) return THREEFOLD_REPETITION;
    return NO_GAME_OVER;
}

static const char *result_for(GameState gs, Color side_to_move)
{
    switch (gs) {
        case CHECKMATE: return side_to_move == WHITE ? "0-1" : "1-0";
        case STALEMATE:
        case INSUFFICIENT_MATERIAL:
        case THREEFOLD_REPETITION:
        case FIFTY_MOVE_RULE: return "1/2-1/2";
        default: return "*";
    }
}

static const char *reason_for(GameState gs)
{
    switch (gs) {
        case CHECKMATE: return "checkmate";
        case STALEMATE: return "stalemate";
        case INSUFFICIENT_MATERIAL: return "insufficient material";
        case THREEFOLD_REPETITION: return "threefold repetition";
        case FIFTY_MOVE_RULE: return "fifty-move rule";
        default: return "game over";
    }
}

/* Append a finished game to $OPENCHESS_RESULTS (one JSON object per line). */
static void log_result(const Room *r, const char *result, const char *reason)
{
    const char *path = getenv("OPENCHESS_RESULTS");
    if (!path || !*path) return;
    FILE *f = fopen(path, "a");
    if (!f) return;
    char fen[128];
    fen_generate(&r->board, fen, sizeof fen);
    fprintf(f, "{\"code\":\"%s\",\"white\":\"%s\",\"black\":\"%s\","
               "\"result\":\"%s\",\"reason\":\"%s\",\"fen\":\"%s\"}\n",
            r->code, r->name[0], r->name[1], result, reason, fen);
    fclose(f);
}

/* End the game without tearing the room down (so a rematch is possible). */
static void finish_game(Room *r, const char *result, const char *reason)
{
    if (!r || r->over) return;
    r->over = 1;
    log_result(r, result, reason);

    /* Rated game: update both Elo ratings before announcing the result. */
    if (r->rated && g_accounts && r->seat[0] >= 0 && r->seat[1] >= 0) {
        Session *w = &sessions[r->seat[0]], *b = &sessions[r->seat[1]];
        if (w->logged_in && b->logged_in) {
            double ea = 1.0 / (1.0 + pow(10.0, (b->pvp_rating - w->pvp_rating) / 400.0));
            double sb, sw;
            if (strcmp(result, "1-0") == 0) { sw = 1.0; sb = 0.0; }
            else if (strcmp(result, "0-1") == 0) { sw = 0.0; sb = 1.0; }
            else { sw = 0.5; sb = 0.5; }
            w->pvp_rating += (int)(32.0 * (sw - ea) + 0.5);
            b->pvp_rating += (int)(32.0 * (sb - (1.0 - ea)) + 0.5);
            if (w->pvp_rating < 400) w->pvp_rating = 400;
            if (b->pvp_rating < 400) b->pvp_rating = 400;
            accounts_set_pvp(g_accounts, w->user_id, w->pvp_rating);
            accounts_set_pvp(g_accounts, b->user_id, b->pvp_rating);
        }
    }

    for (int k = 0; k < 2; k++) {
        if (r->seat[k] < 0) continue;
        Session *t = &sessions[r->seat[k]];
        if (!t->used) continue;
        cJSON *m = proto_new(PROTO_S2C_GAMEOVER);
        cJSON_AddStringToObject(m, "result", result);
        cJSON_AddStringToObject(m, "reason", reason);
        add_clocks(m, r);
        sess_send(m, t);
        if (r->rated && t->logged_in) send_auth(t, true, NULL);   /* new rating */
    }
    /* spectators */
    for (int i = 0; i < MAX_SESS; i++) {
        if (!sessions[i].used || sessions[i].room != (int)(r - rooms) ||
            sessions[i].seat >= 0) continue;
        cJSON *m = proto_new(PROTO_S2C_GAMEOVER);
        cJSON_AddStringToObject(m, "result", result);
        cJSON_AddStringToObject(m, "reason", reason);
        sess_send(m, &sessions[i]);
    }
}

static void broadcast_move(Room *r, const char *uci)
{
    char fen[128];
    fen_generate(&r->board, fen, sizeof fen);
    const char *san = r->ply > 0 ? r->san[r->ply - 1] : "";
    for (int k = 0; k < 2; k++) {
        if (r->seat[k] < 0) continue;
        Session *t = &sessions[r->seat[k]];
        if (!t->used) continue;
        cJSON *m = proto_new(PROTO_S2C_MOVE);
        cJSON_AddStringToObject(m, "uci", uci);
        cJSON_AddStringToObject(m, "san", san);
        cJSON_AddNumberToObject(m, "ply", r->ply - 1);
        cJSON_AddStringToObject(m, "fen", fen);
        add_clocks(m, r);
        sess_send(m, t);
    }
    for (int i = 0; i < MAX_SESS; i++) {
        if (!sessions[i].used || sessions[i].room != (int)(r - rooms) ||
            sessions[i].seat >= 0) continue;
        cJSON *m = proto_new(PROTO_S2C_MOVE);
        cJSON_AddStringToObject(m, "uci", uci);
        cJSON_AddStringToObject(m, "san", san);
        cJSON_AddNumberToObject(m, "ply", r->ply - 1);
        cJSON_AddStringToObject(m, "fen", fen);
        add_clocks(m, r);
        sess_send(m, &sessions[i]);
    }
}

/* ---- reconnect ---------------------------------------------------------- */

/* Reclaim a disconnected seat whose token matches `tok`. */
static bool try_resume(Session *s, const char *tok)
{
    if (!tok || !*tok) return false;
    for (int ri = 0; ri < MAX_ROOMS; ri++) {
        Room *r = &rooms[ri];
        if (!r->used || r->over) continue;
        for (int k = 0; k < 2; k++) {
            if (r->seat[k] < 0 || r->connected[k]) continue;
            if (strcmp(r->seattok[k], tok) != 0) continue;

            r->seat[k] = sidx(s);
            r->connected[k] = 1;
            r->rematch[k] = 0;
            snprintf(r->name[k], sizeof r->name[k], "%s", s->nick);
            snprintf(r->seattok[k], sizeof r->seattok[k], "%s", s->token);
            s->room = ri;
            s->seat = k;

            int opp = r->seat[1 - k];
            if (opp >= 0 && sessions[opp].used) {
                cJSON *m = proto_new(PROTO_S2C_OPPONENT);
                cJSON_AddBoolToObject(m, "connected", true);
                cJSON_AddStringToObject(m, "nick", s->nick);
                sess_send(m, &sessions[opp]);
            }
            if (r->started) send_state(r, s);
            else            send_room(s, r);
            return true;
        }
    }
    return false;
}

/* ---- message handlers --------------------------------------------------- */

static void apply_account(Session *s, const Account *ac)
{
    s->logged_in = true;
    s->user_id = ac->id;
    snprintf(s->user, sizeof s->user, "%s", ac->name);
    snprintf(s->auth_token, sizeof s->auth_token, "%s", ac->token);
    s->pvp_rating = ac->pvp_rating;
    s->puzzle_rating = ac->puzzle_rating;
    snprintf(s->nick, sizeof s->nick, "%s", ac->name);
}

static void handle_hello(Session *s, const ProtoFrame *f)
{
    const char *nick = proto_field_str(f, "nick");
    if (nick && *nick) snprintf(s->nick, sizeof s->nick, "%s", nick);
    const char *tok = proto_field_str(f, "token");
    if (tok && *tok) try_resume(s, tok);
    const char *auth = proto_field_str(f, "auth");
    if (auth && *auth && g_accounts) {
        Account ac;
        if (accounts_login_token(g_accounts, auth, &ac) == 1) {
            apply_account(s, &ac);
            send_auth(s, true, NULL);
        }
    }
}

static void handle_register(Session *s, const ProtoFrame *f)
{
    if (!g_accounts) { send_auth(s, false, "accounts unavailable"); return; }
    const char *u = proto_field_str(f, "user");
    const char *p = proto_field_str(f, "pass");
    Account ac;
    int r = accounts_register(g_accounts, u, p, &ac);
    if (r == 1) { apply_account(s, &ac); send_auth(s, true, NULL); }
    else if (r == 0) send_auth(s, false, "name taken");
    else send_auth(s, false, "registration failed");
}

static void handle_login(Session *s, const ProtoFrame *f)
{
    if (!g_accounts) { send_auth(s, false, "accounts unavailable"); return; }
    const char *tok = proto_field_str(f, "token");
    Account ac;
    if (tok && *tok) {
        int r = accounts_login_token(g_accounts, tok, &ac);
        if (r == 1) { apply_account(s, &ac); send_auth(s, true, NULL); }
        else send_auth(s, false, "session expired");
        return;
    }
    const char *u = proto_field_str(f, "user");
    const char *p = proto_field_str(f, "pass");
    int r = accounts_login(g_accounts, u, p, &ac);
    if (r == 1) { apply_account(s, &ac); send_auth(s, true, NULL); }
    else send_auth(s, false, "wrong user or password");
}

static void handle_logout(Session *s)
{
    if (g_accounts && s->logged_in) accounts_logout(g_accounts, s->auth_token);
    s->logged_in = false;
    s->user_id = 0;
    s->user[0] = '\0';
    s->auth_token[0] = '\0';
    send_auth(s, false, "logged out");
}

static void handle_puzzle_result(Session *s, const ProtoFrame *f)
{
    if (!s->logged_in || !g_accounts) return;
    int pr = proto_field_int(f, "rating", 1500);
    bool solved = proto_field_bool(f, "solved", false);

    double ur = s->puzzle_rating, er = 1.0 / (1.0 + pow(10.0, (pr - ur) / 400.0));
    ur += 32.0 * ((solved ? 1.0 : 0.0) - er);
    if (ur < 400) ur = 400;
    if (ur > 3000) ur = 3000;
    s->puzzle_rating = (int)(ur + 0.5);
    accounts_set_puzzle(g_accounts, s->user_id, s->puzzle_rating);
    send_auth(s, true, NULL);
}

static void handle_create(Session *s, const ProtoFrame *f)
{
    if (s->room >= 0) {
        if (!rooms[s->room].over) { send_reject(s, "already in a game"); return; }
        leave_if_over(s);
    }
    int t = proto_field_int(f, "time", 0);
    int inc = proto_field_int(f, "inc", 0);
    s->want_rated = proto_field_bool(f, "rated", false);
    char code[CODE_LEN + 1];
    int ri = alloc_room(code, t, inc);
    if (ri < 0) { send_reject(s, "server full"); return; }

    s->seat = 0;
    s->room = ri;
    rooms[ri].seat[0] = sidx(s);
    rooms[ri].connected[0] = 1;
    snprintf(rooms[ri].name[0], sizeof rooms[ri].name[0], "%s", s->nick);
    snprintf(rooms[ri].seattok[0], sizeof rooms[ri].seattok[0], "%s", s->token);
    send_room(s, &rooms[ri]);
}

static void handle_join(Session *s, const ProtoFrame *f)
{
    if (s->room >= 0) {
        if (!rooms[s->room].over) { send_reject(s, "already in a game"); return; }
        leave_if_over(s);
    }
    const char *code = proto_field_str(f, "code");
    s->want_rated = proto_field_bool(f, "rated", false);
    int ri = find_room(code);
    if (ri < 0 || rooms[ri].started || rooms[ri].seat[1] >= 0) {
        send_reject(s, "room not available");
        return;
    }
    s->seat = 1;
    s->room = ri;
    rooms[ri].seat[1] = sidx(s);
    rooms[ri].connected[1] = 1;
    snprintf(rooms[ri].name[1], sizeof rooms[ri].name[1], "%s", s->nick);
    snprintf(rooms[ri].seattok[1], sizeof rooms[ri].seattok[1], "%s", s->token);
    send_room(s, &rooms[ri]);
    start_match(&rooms[ri]);
}

static void handle_queue(Session *s, const ProtoFrame *f)
{
    if (s->queued) return;
    if (s->room >= 0) {
        if (!rooms[s->room].over) { send_reject(s, "already in a game"); return; }
        leave_if_over(s);
    }
    s->want_time = proto_field_int(f, "time", 0);
    s->want_inc = proto_field_int(f, "inc", 0);
    s->want_rated = proto_field_bool(f, "rated", false);

    s->queued = 1;
    queue_slots[queue_count++] = sidx(s);

    while (queue_count >= 2) {
        int a = queue_slots[0], b = queue_slots[1];
        memmove(&queue_slots[0], &queue_slots[2],
                sizeof queue_slots[0] * (queue_count - 2));
        queue_count -= 2;
        sessions[a].queued = sessions[b].queued = 0;

        char code[CODE_LEN + 1];
        int ri = alloc_room(code, sessions[a].want_time, sessions[a].want_inc);
        if (ri < 0) continue;
        sessions[a].seat = 0; sessions[a].room = ri;
        sessions[b].seat = 1; sessions[b].room = ri;
        rooms[ri].seat[0] = a;
        rooms[ri].seat[1] = b;
        rooms[ri].connected[0] = rooms[ri].connected[1] = 1;
        snprintf(rooms[ri].name[0], sizeof rooms[ri].name[0], "%s", sessions[a].nick);
        snprintf(rooms[ri].name[1], sizeof rooms[ri].name[1], "%s", sessions[b].nick);
        snprintf(rooms[ri].seattok[0], sizeof rooms[ri].seattok[0], "%s", sessions[a].token);
        snprintf(rooms[ri].seattok[1], sizeof rooms[ri].seattok[1], "%s", sessions[b].token);
        start_match(&rooms[ri]);
    }

    if (s->queued) {
        cJSON *m = proto_new(PROTO_S2C_QUEUED);
        cJSON_AddNumberToObject(m, "position", queue_count);
        sess_send(m, s);
    }
}

static void handle_cancel_queue(Session *s)
{
    if (!s->queued) return;
    queue_remove(sidx(s));
    s->queued = 0;
}

static void handle_move(Session *s, const ProtoFrame *f)
{
    if (s->room < 0) return;
    Room *r = &rooms[s->room];
    if (!r->started || r->over) return;

    int mover = s->seat;
    int turn  = (r->board.side == WHITE) ? 0 : 1;
    if (mover != turn) { send_reject(s, "not your turn"); send_state(r, s); return; }

    const char *uci = proto_field_str(f, "uci");
    Move m;
    if (!uci_to_move(&r->board, uci, &m) || !is_legal(&r->board, m)) {
        send_reject(s, "illegal move");
        send_state(r, s);
        return;
    }

    Board pre = r->board;
    move_to_san(&pre, m, r->san[r->ply], sizeof r->san[r->ply]);
    make_move_plumb(&r->board, m);
    r->history[r->ply++] = m;
    r->pos[r->ply] = r->board;
    r->npos = r->ply + 1;

    if (r->clock_ms[0] >= 0) {
        uint64_t elapsed = now_ms() - r->last_ts;
        r->clock_ms[mover] -= (int)elapsed;
        if (r->clock_ms[mover] < 0) r->clock_ms[mover] = 0;
        if (r->clock_ms[mover] == 0) {
            broadcast_move(r, uci);
            finish_game(r, mover == 0 ? "0-1" : "1-0", "timeout");
            return;
        }
        r->clock_ms[mover] += r->inc_ms;
        r->last_ts = now_ms();
    }

    broadcast_move(r, uci);

    GameState gs = referee_state(r);
    if (gs != NO_GAME_OVER)
        finish_game(r, result_for(gs, r->board.side), reason_for(gs));
}

static void handle_resign(Session *s)
{
    if (s->room < 0 || s->seat < 0) return;
    Room *r = &rooms[s->room];
    if (!r->started || r->over) return;
    finish_game(r, s->seat == 0 ? "0-1" : "1-0", "resign");
}

static void handle_rematch(Session *s)
{
    if (s->room < 0 || s->seat < 0) return;
    Room *r = &rooms[s->room];
    if (!r->started || !r->over) return;
    r->rematch[s->seat] = 1;

    int opp = r->seat[1 - s->seat];
    if (!(r->rematch[0] && r->rematch[1])) {
        if (opp >= 0 && sessions[opp].used) {
            cJSON *m = proto_new(PROTO_S2C_REMATCH);
            cJSON_AddBoolToObject(m, "wants", true);
            sess_send(m, &sessions[opp]);
        }
        return;
    }

    /* Both agreed: swap colours, reset the board and clocks, start again. */
    int t = r->seat[0]; r->seat[0] = r->seat[1]; r->seat[1] = t;
    char tt[40];
    memcpy(tt, r->seattok[0], sizeof tt);
    memcpy(r->seattok[0], r->seattok[1], sizeof tt);
    memcpy(r->seattok[1], tt, sizeof tt);
    memcpy(tt, r->name[0], sizeof tt);
    memcpy(r->name[0], r->name[1], sizeof tt);
    memcpy(r->name[1], tt, sizeof tt);
    sessions[r->seat[0]].seat = 0;
    sessions[r->seat[1]].seat = 1;

    board_reset(&r->board);
    r->ply = 0;
    r->pos[0] = r->board;
    r->npos = 1;
    r->over = 0;
    r->rematch[0] = r->rematch[1] = 0;
    r->connected[0] = r->connected[1] = 1;
    r->clock_ms[0] = r->clock_ms[1] = r->init_time > 0 ? r->init_time : -1;
    start_match(r);
}

static void handle_ping(Session *s, const ProtoFrame *f)
{
    cJSON *m = proto_new(PROTO_S2C_PONG);
    cJSON_AddNumberToObject(m, "t", proto_field_int(f, "t", 0));
    sess_send(m, s);
}

/* Relay a draw offer/decline to the opponent; accepting ends the game. */
static void handle_draw(Session *s, const char *action)
{
    if (s->room < 0 || s->seat < 0) return;
    Room *r = &rooms[s->room];
    if (!r->started || r->over) return;
    if (strcmp(action, "accept") == 0) {
        finish_game(r, "1/2-1/2", "agreement");
        return;
    }
    int opp = r->seat[1 - s->seat];
    if (opp >= 0 && sessions[opp].used) {
        cJSON *m = proto_new(PROTO_S2C_DRAW);
        cJSON_AddStringToObject(m, "action", action);
        sess_send(m, &sessions[opp]);
    }
}

/* Relay chat to the opponent and any spectators. */
static void handle_chat(Session *s, const ProtoFrame *f)
{
    if (s->room < 0) return;
    Room *r = &rooms[s->room];
    const char *text = proto_field_str(f, "text");
    if (!text || !*text) return;

    /* Simple rate limit: at most 5 lines per 5 seconds per session. */
    uint64_t now = now_ms();
    if (now - s->chat_win > 5000) { s->chat_win = now; s->chat_count = 0; }
    if (++s->chat_count > 5) return;

    char clipped[160];
    snprintf(clipped, sizeof clipped, "%.150s", text);

    if (s->seat >= 0) {
        int opp = r->seat[1 - s->seat];
        if (opp >= 0 && sessions[opp].used) {
            cJSON *m = proto_new(PROTO_S2C_CHAT);
            cJSON_AddStringToObject(m, "from", s->nick);
            cJSON_AddStringToObject(m, "text", clipped);
            sess_send(m, &sessions[opp]);
        }
    }
    for (int i = 0; i < MAX_SESS; i++) {
        if (!sessions[i].used || sessions[i].room != s->room ||
            sessions[i].seat >= 0) continue;
        cJSON *m = proto_new(PROTO_S2C_CHAT);
        cJSON_AddStringToObject(m, "from", s->nick);
        cJSON_AddStringToObject(m, "text", clipped);
        sess_send(m, &sessions[i]);
    }
}

/* Join a running game as a read-only spectator. */
static void handle_spectate(Session *s, const ProtoFrame *f)
{
    const char *code = proto_field_str(f, "code");
    int ri = find_room(code);
    if (ri < 0 || !rooms[ri].started) { send_reject(s, "no such game"); return; }
    s->room = ri;
    s->seat = -1;

    Room *r = &rooms[ri];
    char fen[128];
    fen_generate(&r->board, fen, sizeof fen);
    cJSON *m = proto_new(PROTO_S2C_SPECTATE);
    cJSON_AddStringToObject(m, "code", r->code);
    cJSON_AddStringToObject(m, "fen", fen);
    cJSON_AddStringToObject(m, "side", r->board.side == WHITE ? "white" : "black");
    cJSON_AddNumberToObject(m, "ply", r->ply);
    cJSON_AddBoolToObject(m, "over", r->over != 0);
    cJSON_AddStringToObject(m, "white", r->name[0]);
    cJSON_AddStringToObject(m, "black", r->name[1]);
    add_clocks(m, r);
    sess_send(m, s);
}

/* ---- lws callback ------------------------------------------------------- */

static Session *session_from_user(void *user)
{
    PerConn *pc = user;
    if (!pc || pc->idx < 0 || pc->idx >= MAX_SESS) return NULL;
    return &sessions[pc->idx];
}

static int on_established(void *user, struct lws *wsi)
{
    PerConn *pc = user;
    int slot = -1;
    for (int i = 0; i < MAX_SESS; i++)
        if (!sessions[i].used) { slot = i; break; }
    if (slot < 0) return -1;
    memset(&sessions[slot], 0, sizeof sessions[slot]);
    sessions[slot].used = 1;
    sessions[slot].wsi = wsi;
    sessions[slot].room = -1;
    sessions[slot].seat = -1;
    sessions[slot].last_rx = now_ms();
    sessions[slot].last_ping = now_ms();
    rand_token(sessions[slot].token, sizeof sessions[slot].token);
    snprintf(sessions[slot].nick, sizeof sessions[slot].nick, "Player");
    pc->idx = slot;
    send_welcome(&sessions[slot]);
    return 0;
}

static void on_closed(Session *s)
{
    if (!s) return;
    if (s->queued) queue_remove(sidx(s));

    if (s->room >= 0) {
        Room *r = &rooms[s->room];
        if (r->used) {
            if (r->over) {
                detach_room(s->room);       /* finished game: no grace needed */
            } else {
                int seat = s->seat;
                if (seat >= 0 && r->seat[seat] == sidx(s)) {
                    r->connected[seat] = 0;
                    r->dc_ts[seat] = now_ms();
                    int opp = r->seat[1 - seat];
                    if (opp >= 0 && sessions[opp].used) {
                        cJSON *m = proto_new(PROTO_S2C_OPPONENT);
                        cJSON_AddBoolToObject(m, "connected", false);
                        cJSON_AddStringToObject(m, "nick", s->nick);
                        sess_send(m, &sessions[opp]);
                    }
                }
            }
        }
    }
    s->used = 0;
    s->wsi = NULL;
}

static int srv_cb(struct lws *wsi, enum lws_callback_reasons reason,
                  void *user, void *in, size_t len)
{
    Session *s = session_from_user(user);

    switch (reason) {
    case LWS_CALLBACK_ESTABLISHED:
        return on_established(user, wsi);

    case LWS_CALLBACK_CLOSED:
        on_closed(s);
        return 0;

    case LWS_CALLBACK_SERVER_WRITEABLE: {
        if (!s) return 0;
        while (s->qn > 0) {
            char msg[SRV_LINELEN];
            memcpy(msg, s->outq[s->qh], sizeof msg);
            size_t n = strlen(msg);
            unsigned char *buf = malloc(LWS_PRE + n);
            if (!buf) break;
            memcpy(buf + LWS_PRE, msg, n);
            int m = lws_write(wsi, buf + LWS_PRE, n, LWS_WRITE_TEXT);
            free(buf);
            if (m < 0) break;
            s->qh = (s->qh + 1) % SRV_QLEN;
            s->qn--;
        }
        return 0;
    }

    case LWS_CALLBACK_RECEIVE: {
        if (!s) return 0;
        s->last_rx = now_ms();
        char text[SRV_LINELEN];
        size_t n = len < sizeof text - 1 ? len : sizeof text - 1;
        memcpy(text, in, n);
        text[n] = '\0';

        ProtoFrame f;
        if (!proto_parse_dir(text, PROTO_DIR_C2S, &f)) return 0;
        switch (f.type) {
        case PROTO_C2S_HELLO:        handle_hello(s, &f); break;
        case PROTO_C2S_CREATE:       handle_create(s, &f); break;
        case PROTO_C2S_JOIN:         handle_join(s, &f); break;
        case PROTO_C2S_QUEUE:        handle_queue(s, &f); break;
        case PROTO_C2S_CANCEL_QUEUE: handle_cancel_queue(s); break;
        case PROTO_C2S_MOVE:         handle_move(s, &f); break;
        case PROTO_C2S_RESIGN:       handle_resign(s); break;
        case PROTO_C2S_REMATCH:      handle_rematch(s); break;
        case PROTO_C2S_DRAW_OFFER:   handle_draw(s, "offer"); break;
        case PROTO_C2S_DRAW_ACCEPT:  handle_draw(s, "accept"); break;
        case PROTO_C2S_DRAW_DECLINE: handle_draw(s, "decline"); break;
        case PROTO_C2S_CHAT:         handle_chat(s, &f); break;
        case PROTO_C2S_SPECTATE:     handle_spectate(s, &f); break;
        case PROTO_C2S_REGISTER:     handle_register(s, &f); break;
        case PROTO_C2S_LOGIN:        handle_login(s, &f); break;
        case PROTO_C2S_LOGOUT:       handle_logout(s); break;
        case PROTO_C2S_PUZZLE_RESULT:handle_puzzle_result(s, &f); break;
        case PROTO_C2S_PING:         handle_ping(s, &f); break;
        case PROTO_C2S_PONG:         break;   /* heartbeat reply */
        default: break;
        }
        proto_frame_free(&f);
        return 0;
    }

    default:
        return 0;
    }
}

/* ---- main --------------------------------------------------------------- */

static volatile int g_stop = 0;
static struct lws_context *g_ctx = NULL;
static void on_signal(int sig) { (void)sig; g_stop = 1; }

/* lws_service() can block in poll() while a dead peer is being reaped; a wakeup
 * ticker keeps the main loop running so clocks/grace are enforced on time. */
static void *wake_ticker(void *arg)
{
    (void)arg;
    while (!g_stop) {
        usleep(20000);
        if (g_ctx) lws_cancel_service(g_ctx);
    }
    return NULL;
}

/* Clock flags, disconnect grace/abandonment and the heartbeat watchdog. */
static void check_timeouts(void)
{
    uint64_t now = now_ms();

    for (int i = 0; i < MAX_ROOMS; i++) {
        Room *r = &rooms[i];
        if (!r->used) continue;

        if (r->started && !r->over && r->clock_ms[0] >= 0) {
            int turn = (r->board.side == WHITE) ? 0 : 1;
            int remaining = r->clock_ms[turn] - (int)(now - r->last_ts);
            if (remaining <= 0) {
                r->clock_ms[turn] = 0;
                finish_game(r, turn == 0 ? "0-1" : "1-0", "timeout");
            }
        }

        if (r->over) {
            if ((r->seat[0] < 0 || !r->connected[0]) &&
                (r->seat[1] < 0 || !r->connected[1])) {
                detach_room(i);
            }
            continue;
        }

        /* Abandonment: a seat has been gone longer than the grace period. */
        for (int k = 0; k < 2; k++) {
            if (r->seat[k] < 0 || r->connected[k]) continue;
            if (now - r->dc_ts[k] <= (uint64_t)g_grace_ms) continue;

            if (r->started) {
                finish_game(r, k == 0 ? "0-1" : "1-0", "abandoned");
            }
            detach_room(i);
            break;
        }
    }

    for (int i = 0; i < MAX_SESS; i++) {
        Session *s = &sessions[i];
        if (!s->used || !s->wsi) continue;
        if (now - s->last_rx > RX_TIMEOUT_MS) {
            lws_wsi_close(s->wsi, LWS_TO_KILL_ASYNC);
        }
        else if (!s->room && !s->queued && now - s->last_ping > PING_MS) {
            cJSON *m = proto_new(PROTO_S2C_PING);
            cJSON_AddNumberToObject(m, "t", (int)(now & 0x7fffffff));
            sess_send(m, s);
            s->last_ping = now;
        }
    }
}

int main(int argc, char **argv)
{
    int port = 7681;
    const char *env = getenv("OPENCHESS_PORT");
    if (argc > 1) port = atoi(argv[1]);
    else if (env && *env) port = atoi(env);
    if (port <= 0 || port > 65535) port = 7681;

    const char *grace = getenv("OPENCHESS_GRACE_MS");
    if (grace && *grace) {
        int g = atoi(grace);
        if (g > 0) g_grace_ms = g;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    for (int i = 0; i < MAX_ROOMS; i++) rooms[i].seat[0] = rooms[i].seat[1] = -1;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = port;
    info.protocols = SRV_PROTOCOLS;
    info.gid = -1;
    info.uid = -1;

    if (getenv("OPENCHESS_QUIET")) lws_set_log_level(LLL_ERR | LLL_WARN, NULL);

    const char *db = getenv("OPENCHESS_ACCOUNTS_DB");
    if (!db || !*db) db = "openchess_accounts.db";
    g_accounts = accounts_open(db);
    if (!g_accounts)
        fprintf(stderr, "openchessd: accounts unavailable (no SQLite store)\n");

    struct lws_context *ctx = lws_create_context(&info);
    if (!ctx) {
        fprintf(stderr, "openchessd: could not listen on port %d\n", port);
        return 1;
    }
    g_ctx = ctx;
    pthread_t ticker;
    if (pthread_create(&ticker, NULL, wake_ticker, NULL) == 0)
        pthread_detach(ticker);

    printf("openchessd listening on port %d\n", port);
    fflush(stdout);

    /* lws_service()'s own timeout can block in poll() with some builds, so
     * drive it non-blocking and pace the loop ourselves; wake_ticker() also
     * nudges it so a dead peer cannot stall clock/grace handling. */
    while (!g_stop) {
        lws_service(ctx, 0);
        check_timeouts();
        usleep(20000);              /* 50 Hz */
    }

    g_ctx = NULL;                   /* stop the ticker touching the context */
    usleep(40000);
    lws_context_destroy(ctx);
    accounts_close(g_accounts);
    return 0;
}
