#include "online.h"
#include "net.h"
#include "proto.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ONLINE_EVQ 32

#define HEARTBEAT_MS        5000
#define RX_TIMEOUT_MS      15000
#define RECONNECT_DELAY_MS  1000
#define RECONNECT_WINDOW_MS 3000
#define RECONNECT_MAX          5

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

typedef struct {
    OnlineEvent kind;
    char uci[8];
    int  ply;
} Ev;

struct OnlineSession {
    Transport *net;
    OnlineState state;
    Color  color;
    char   code[8];
    char   opponent[40];
    char   error[96];
    char   result[8];
    char   reason[32];
    char   fen[128];
    int    auth_ply;
    int    clock_ms[2];
    bool   clock_timed;
    Color  turn;
    uint64_t clock_ts;

    /* reconnect */
    char   url[128];
    char   nick[40];
    char   token[40];        /* latest welcome token */
    char   resume[40];       /* token sent in hello to reclaim a seat */
    char   auth_token[80];   /* account session token */
    bool   logged_in;
    bool   rated;            /* request a rated game */
    char   user[40];
    int    pvp_rating;
    int    puzzle_rating;
    bool   had_room;
    bool   reconnecting;
    int    attempts;
    uint64_t reconnect_at;
    uint64_t reconnect_deadline;

    bool   spectating;

    struct { char from[40]; char text[160]; } chat[16];
    int    chat_h, chat_t, chat_c;

    uint64_t last_rx;
    uint64_t last_ping;

    Ev     evq[ONLINE_EVQ];
    int    evh, evt, evc;
};

static void apply_clocks(OnlineSession *o, const ProtoFrame *f)
{
    int w = proto_field_int(f, "wtime", -1);
    int b = proto_field_int(f, "btime", -1);
    if (w >= 0 && b >= 0) {
        o->clock_ms[0] = w;
        o->clock_ms[1] = b;
        o->clock_timed = true;
        o->clock_ts = now_ms();
    }
}

static void apply_fen(OnlineSession *o, const ProtoFrame *f)
{
    const char *fen = proto_field_str(f, "fen");
    if (fen && *fen) snprintf(o->fen, sizeof o->fen, "%s", fen);
}

static void emit(OnlineSession *o, OnlineEvent kind)
{
    if (o->evc >= ONLINE_EVQ) return;
    o->evq[o->evt].kind = kind;
    o->evq[o->evt].uci[0] = '\0';
    o->evq[o->evt].ply = 0;
    o->evt = (o->evt + 1) % ONLINE_EVQ;
    o->evc++;
}

static void emit_move(OnlineSession *o, const char *uci, int ply)
{
    if (o->evc >= ONLINE_EVQ) return;
    Ev *e = &o->evq[o->evt];
    e->kind = ONLINE_EV_MOVE;
    snprintf(e->uci, sizeof e->uci, "%s", uci ? uci : "");
    e->ply = ply;
    o->evt = (o->evt + 1) % ONLINE_EVQ;
    o->evc++;
}

static void send_json(OnlineSession *o, cJSON *msg)
{
    char *s = proto_serialize(msg);
    if (!s) return;
    if (!transport_send(o->net, s))
        snprintf(o->error, sizeof o->error, "send failed");
    free(s);
}

static void send_hello(OnlineSession *o)
{
    cJSON *h = proto_new(PROTO_C2S_HELLO);
    cJSON_AddStringToObject(h, "nick", o->nick[0] ? o->nick : "Player");
    if (o->resume[0]) cJSON_AddStringToObject(h, "token", o->resume);
    if (o->auth_token[0]) cJSON_AddStringToObject(h, "auth", o->auth_token);
    send_json(o, h);
}

OnlineSession *online_create(const char *url, const char *nick,
                             const char *resume_token, const char *auth_token)
{
    if (!url || !*url) return NULL;
    OnlineSession *o = calloc(1, sizeof *o);
    if (!o) return NULL;
    snprintf(o->url, sizeof o->url, "%s", url);
    snprintf(o->nick, sizeof o->nick, "%s", nick && *nick ? nick : "Player");
    snprintf(o->resume, sizeof o->resume, "%s", resume_token ? resume_token : "");
    snprintf(o->auth_token, sizeof o->auth_token, "%s", auth_token ? auth_token : "");
    o->net = net_ws_connect(url);
    if (!o->net) { free(o); return NULL; }
    o->state = ONLINE_CONNECTING;
    o->color = WHITE;
    o->last_rx = o->last_ping = now_ms();
    send_hello(o);
    return o;
}

void online_destroy(OnlineSession *o)
{
    if (!o) return;
    if (o->net) transport_close(o->net);
    free(o);
}

static void on_frame(OnlineSession *o, ProtoFrame *f)
{
    o->last_rx = now_ms();
    switch (f->type) {
    case PROTO_S2C_WELCOME: {
        const char *tok = proto_field_str(f, "token");
        if (tok) {
            snprintf(o->token, sizeof o->token, "%s", tok);
            snprintf(o->resume, sizeof o->resume, "%s", tok);
        }
        if (!o->reconnecting) o->state = ONLINE_IDLE;
        break;
    }
    case PROTO_S2C_ROOM: {
        const char *code = proto_field_str(f, "code");
        if (code) snprintf(o->code, sizeof o->code, "%s", code);
        const char *col = proto_field_str(f, "color");
        o->color = (col && strcmp(col, "black") == 0) ? BLACK : WHITE;
        apply_clocks(o, f);
        o->had_room = true;
        o->reconnecting = false;
        o->attempts = 0;
        o->state = ONLINE_HOSTING;
        break;
    }
    case PROTO_S2C_QUEUED:
        o->state = ONLINE_QUEUED;
        break;
    case PROTO_S2C_START: {
        const char *col = proto_field_str(f, "color");
        o->color = (col && strcmp(col, "black") == 0) ? BLACK : WHITE;
        const char *opp = proto_field_str(f, "opponent");
        snprintf(o->opponent, sizeof o->opponent, "%s", opp ? opp : "Opponent");
        const char *code = proto_field_str(f, "code");
        if (code) snprintf(o->code, sizeof o->code, "%s", code);
        apply_fen(o, f);
        apply_clocks(o, f);
        o->turn = WHITE;
        o->auth_ply = 0;
        o->had_room = true;
        o->reconnecting = false;
        o->attempts = 0;
        o->state = ONLINE_PLAYING;
        emit(o, ONLINE_EV_START);
        break;
    }
    case PROTO_S2C_MOVE: {
        const char *uci = proto_field_str(f, "uci");
        apply_fen(o, f);
        apply_clocks(o, f);
        o->turn = (o->turn == WHITE) ? BLACK : WHITE;
        o->auth_ply = proto_field_int(f, "ply", o->auth_ply) + 1;
        emit_move(o, uci, proto_field_int(f, "ply", 0));
        break;
    }
    case PROTO_S2C_STATE: {
        apply_fen(o, f);
        apply_clocks(o, f);
        const char *side = proto_field_str(f, "side");
        o->turn = (side && strcmp(side, "black") == 0) ? BLACK : WHITE;
        o->auth_ply = proto_field_int(f, "ply", o->auth_ply);
        o->had_room = true;
        o->reconnecting = false;
        o->attempts = 0;
        o->state = ONLINE_PLAYING;
        emit(o, ONLINE_EV_STATE);
        break;
    }
    case PROTO_S2C_REJECT: {
        const char *r = proto_field_str(f, "reason");
        snprintf(o->error, sizeof o->error, "%s", r ? r : "rejected");
        emit(o, ONLINE_EV_REJECT);
        break;
    }
    case PROTO_S2C_GAMEOVER: {
        const char *res = proto_field_str(f, "result");
        const char *rea = proto_field_str(f, "reason");
        snprintf(o->result, sizeof o->result, "%s", res ? res : "");
        snprintf(o->reason, sizeof o->reason, "%s", rea ? rea : "");
        apply_clocks(o, f);
        emit(o, ONLINE_EV_GAMEOVER);
        break;
    }
    case PROTO_S2C_REMATCH:
        emit(o, ONLINE_EV_REMATCH);
        break;
    case PROTO_S2C_DRAW: {
        const char *action = proto_field_str(f, "action");
        if (action && strcmp(action, "offer") == 0) emit(o, ONLINE_EV_DRAW_OFFER);
        else if (action && strcmp(action, "decline") == 0) emit(o, ONLINE_EV_DRAW_DECLINE);
        break;
    }
    case PROTO_S2C_CHAT: {
        if (o->chat_c < (int)(sizeof o->chat / sizeof o->chat[0])) {
            snprintf(o->chat[o->chat_t].from, 40, "%s",
                     proto_field_str(f, "from") ? proto_field_str(f, "from") : "?");
            snprintf(o->chat[o->chat_t].text, 160, "%s",
                     proto_field_str(f, "text") ? proto_field_str(f, "text") : "");
            o->chat_t = (o->chat_t + 1) % (int)(sizeof o->chat / sizeof o->chat[0]);
            o->chat_c++;
        }
        break;
    }
    case PROTO_S2C_SPECTATE: {
        const char *code = proto_field_str(f, "code");
        if (code) snprintf(o->code, sizeof o->code, "%s", code);
        apply_fen(o, f);
        apply_clocks(o, f);
        const char *side = proto_field_str(f, "side");
        o->turn = (side && strcmp(side, "black") == 0) ? BLACK : WHITE;
        o->auth_ply = proto_field_int(f, "ply", 0);
        o->spectating = true;
        o->state = ONLINE_PLAYING;
        emit(o, ONLINE_EV_SPECTATE);
        break;
    }
    case PROTO_S2C_OPPONENT: {
        bool connected = proto_field_bool(f, "connected", true);
        if (!connected) {
            snprintf(o->opponent, sizeof o->opponent, "%s",
                     proto_field_str(f, "nick") ? proto_field_str(f, "nick") : "Opponent");
            emit(o, ONLINE_EV_OPPONENT_LEFT);
        } else {
            emit(o, ONLINE_EV_OPPONENT_JOINED);
        }
        break;
    }
    case PROTO_S2C_AUTH: {
        bool ok = proto_field_bool(f, "ok", false);
        if (ok) {
            const char *u = proto_field_str(f, "user");
            const char *tk = proto_field_str(f, "token");
            snprintf(o->user, sizeof o->user, "%s", u ? u : "");
            if (tk && *tk) snprintf(o->auth_token, sizeof o->auth_token, "%s", tk);
            o->pvp_rating = proto_field_int(f, "pvp_rating", 1500);
            o->puzzle_rating = proto_field_int(f, "puzzle_rating", 1500);
            o->logged_in = true;
            snprintf(o->nick, sizeof o->nick, "%s", o->user);
        } else {
            o->logged_in = false;
            const char *r = proto_field_str(f, "reason");
            if (r && strcmp(r, "logged out") != 0)
                snprintf(o->error, sizeof o->error, "%s", r);
        }
        emit(o, ONLINE_EV_AUTH);
        break;
    }
    case PROTO_S2C_PING:
        /* Reply so the server's rx watchdog stays happy. */
        send_json(o, proto_new(PROTO_C2S_PONG));
        break;
    default:
        break;
    }
}

static void begin_reconnect(OnlineSession *o, uint64_t now)
{
    if (o->reconnecting) return;
    o->reconnecting = true;
    o->reconnect_at = now + RECONNECT_DELAY_MS;
    if (!o->error[0]) snprintf(o->error, sizeof o->error, "connection lost");
}

void online_poll(OnlineSession *o)
{
    if (!o) return;
    uint64_t now = now_ms();

    if (o->net) {
        char line[1024];
        int r;
        while ((r = transport_poll(o->net, line, sizeof line)) == 1) {
            ProtoFrame f;
            if (proto_parse_dir(line, PROTO_DIR_S2C, &f)) {
                on_frame(o, &f);
                proto_frame_free(&f);
            }
        }
        if (r < 0 && o->state != ONLINE_CLOSED) {
            transport_close(o->net);
            o->net = NULL;
            o->state = ONLINE_CLOSED;
            if (o->had_room && o->resume[0]) begin_reconnect(o, now);
        }

        if (o->net && o->state != ONLINE_CLOSED &&
            now - o->last_ping > HEARTBEAT_MS) {
            cJSON *p = proto_new(PROTO_C2S_PING);
            cJSON_AddNumberToObject(p, "t", (int)(now & 0x7fffffff));
            send_json(o, p);
            o->last_ping = now;
        }
        if (o->net && o->state != ONLINE_CLOSED &&
            now - o->last_rx > RX_TIMEOUT_MS) {
            transport_close(o->net);
            o->net = NULL;
            o->state = ONLINE_CLOSED;
            snprintf(o->error, sizeof o->error, "connection timed out");
            if (o->had_room && o->resume[0]) begin_reconnect(o, now);
        }
    }

    /* Automatic reconnect for a game/room we were in. */
    if (o->reconnecting) {
        if (!o->net && now >= o->reconnect_at) {
            o->net = net_ws_connect(o->url);
            if (o->net) {
                o->state = ONLINE_CONNECTING;
                o->attempts++;
                o->reconnect_deadline = now + RECONNECT_WINDOW_MS;
                send_hello(o);
            } else {
                o->reconnect_at = now + RECONNECT_DELAY_MS;
            }
        } else if (o->net && o->state == ONLINE_CONNECTING &&
                   now > o->reconnect_deadline) {
            /* No resume within the window; retry or give up. */
            transport_close(o->net);
            o->net = NULL;
            o->state = ONLINE_CLOSED;
            if (o->attempts >= RECONNECT_MAX) {
                o->reconnecting = false;
                snprintf(o->error, sizeof o->error, "could not reconnect");
            } else {
                o->reconnect_at = now + RECONNECT_DELAY_MS;
            }
        }
    }
}

OnlineState online_state(const OnlineSession *o) { return o ? o->state : ONLINE_CLOSED; }
const char *online_room_code(const OnlineSession *o) { return o ? o->code : ""; }
const char *online_opponent(const OnlineSession *o) { return o ? o->opponent : ""; }
const char *online_last_error(const OnlineSession *o) { return o ? o->error : ""; }
const char *online_result(const OnlineSession *o) { return o ? o->result : ""; }
Color       online_color(const OnlineSession *o) { return o ? o->color : WHITE; }
const char *online_fen(const OnlineSession *o) { return o ? o->fen : ""; }
int         online_ply(const OnlineSession *o) { return o ? o->auth_ply : 0; }
bool        online_has_clocks(const OnlineSession *o) { return o && o->clock_timed; }
bool        online_reconnecting(const OnlineSession *o) { return o && o->reconnecting; }

void online_clocks(const OnlineSession *o, int *white_ms, int *black_ms)
{
    int w = 0, b = 0;
    if (o && o->clock_timed) {
        uint64_t elapsed = now_ms() - o->clock_ts;
        w = o->clock_ms[0];
        b = o->clock_ms[1];
        if (o->turn == WHITE) w -= (int)elapsed; else b -= (int)elapsed;
        if (w < 0) w = 0;
        if (b < 0) b = 0;
    }
    if (white_ms) *white_ms = w;
    if (black_ms) *black_ms = b;
}

void online_set_rated(OnlineSession *o, bool rated)
{
    if (o) o->rated = rated;
}

void online_create_room(OnlineSession *o, int time_ms, int inc_ms)
{
    if (!o) return;
    cJSON *m = proto_new(PROTO_C2S_CREATE);
    if (time_ms > 0) {
        cJSON_AddNumberToObject(m, "time", time_ms);
        cJSON_AddNumberToObject(m, "inc", inc_ms);
    }
    if (o->rated) cJSON_AddBoolToObject(m, "rated", true);
    send_json(o, m);
}

void online_join_room(OnlineSession *o, const char *code)
{
    if (!o || !code || !*code) return;
    cJSON *m = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(m, "code", code);
    if (o->rated) cJSON_AddBoolToObject(m, "rated", true);
    send_json(o, m);
}

void online_queue(OnlineSession *o, int time_ms, int inc_ms)
{
    if (!o) return;
    cJSON *m = proto_new(PROTO_C2S_QUEUE);
    if (time_ms > 0) {
        cJSON_AddNumberToObject(m, "time", time_ms);
        cJSON_AddNumberToObject(m, "inc", inc_ms);
    }
    if (o->rated) cJSON_AddBoolToObject(m, "rated", true);
    send_json(o, m);
}

void online_cancel_queue(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_CANCEL_QUEUE));
}

void online_send_move(OnlineSession *o, const char *uci, int ply)
{
    if (!o || !uci || !*uci) return;
    cJSON *m = proto_new(PROTO_C2S_MOVE);
    cJSON_AddStringToObject(m, "uci", uci);
    cJSON_AddNumberToObject(m, "ply", ply);
    send_json(o, m);
}

void online_resign(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_RESIGN));
}

void online_rematch(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_REMATCH));
}

void online_draw_offer(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_DRAW_OFFER));
}

void online_draw_accept(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_DRAW_ACCEPT));
}

void online_draw_decline(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_DRAW_DECLINE));
}

void online_send_chat(OnlineSession *o, const char *text)
{
    if (!o || !text || !*text) return;
    cJSON *m = proto_new(PROTO_C2S_CHAT);
    cJSON_AddStringToObject(m, "text", text);
    send_json(o, m);
}

void online_register(OnlineSession *o, const char *user, const char *pass)
{
    if (!o || !user || !*user || !pass || !*pass) return;
    cJSON *m = proto_new(PROTO_C2S_REGISTER);
    cJSON_AddStringToObject(m, "user", user);
    cJSON_AddStringToObject(m, "pass", pass);
    send_json(o, m);
}

void online_login(OnlineSession *o, const char *user, const char *pass)
{
    if (!o || !user || !*user || !pass || !*pass) return;
    cJSON *m = proto_new(PROTO_C2S_LOGIN);
    cJSON_AddStringToObject(m, "user", user);
    cJSON_AddStringToObject(m, "pass", pass);
    send_json(o, m);
}

void online_logout(OnlineSession *o)
{
    if (o) send_json(o, proto_new(PROTO_C2S_LOGOUT));
}

void online_puzzle_result(OnlineSession *o, int puzzle_rating, bool solved)
{
    if (!o || !o->logged_in) return;
    cJSON *m = proto_new(PROTO_C2S_PUZZLE_RESULT);
    cJSON_AddNumberToObject(m, "rating", puzzle_rating);
    cJSON_AddBoolToObject(m, "solved", solved);
    send_json(o, m);
}

bool        online_logged_in(const OnlineSession *o) { return o && o->logged_in; }
const char *online_username(const OnlineSession *o) { return o ? o->user : ""; }
const char *online_auth_token(const OnlineSession *o) { return o ? o->auth_token : ""; }
int         online_pvp_rating(const OnlineSession *o) { return o ? o->pvp_rating : 1500; }
int         online_puzzle_rating(const OnlineSession *o) { return o ? o->puzzle_rating : 1500; }

void online_spectate(OnlineSession *o, const char *code)
{
    if (!o || !code || !*code) return;
    cJSON *m = proto_new(PROTO_C2S_SPECTATE);
    cJSON_AddStringToObject(m, "code", code);
    send_json(o, m);
}

bool online_spectating(const OnlineSession *o) { return o && o->spectating; }

bool online_take_chat(OnlineSession *o, char *from, size_t from_cap,
                      char *text, size_t text_cap)
{
    if (!o || o->chat_c == 0) return false;
    int n = (int)(sizeof o->chat / sizeof o->chat[0]);
    if (from && from_cap) snprintf(from, from_cap, "%s", o->chat[o->chat_h].from);
    if (text && text_cap) snprintf(text, text_cap, "%s", o->chat[o->chat_h].text);
    o->chat_h = (o->chat_h + 1) % n;
    o->chat_c--;
    return true;
}

OnlineEvent online_next_event(OnlineSession *o, char *uci, size_t cap, int *ply)
{
    if (!o || o->evc == 0) return ONLINE_EV_NONE;
    Ev *e = &o->evq[o->evh];
    OnlineEvent kind = e->kind;
    if (kind == ONLINE_EV_MOVE) {
        if (uci && cap) snprintf(uci, cap, "%s", e->uci);
        if (ply) *ply = e->ply;
    }
    o->evh = (o->evh + 1) % ONLINE_EVQ;
    o->evc--;
    return kind;
}
