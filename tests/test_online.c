/*
 * Online multiplayer integration test. Starts the real server binary, connects
 * two WebSocket clients through the actual net_ws transport, and exercises the
 * lobby (create/join + matchmaking) and move relay.
 *
 * Requires HAVE_WS and a built ./server/openchessd (both ensured by the
 * Makefile; the test is skipped otherwise).
 */

#include "../src/net.h"
#include "../src/proto.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void nap(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static unsigned long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}

/* Wait for the next frame of `type`, discarding others. */
static bool expect(Transport *t, ProtoMsg type, int timeout_ms, ProtoFrame *out)
{
    unsigned long deadline = now_ms() + (unsigned long)timeout_ms;
    while (now_ms() < deadline) {
        char line[1024];
        int r = transport_poll(t, line, sizeof line);
        if (r == 1) {
            ProtoFrame f; memset(&f, 0, sizeof f);
            if (proto_parse_dir(line, PROTO_DIR_S2C, &f)) {
                if (f.type == type) { *out = f; return true; }
                proto_frame_free(&f);
            }
        } else if (r < 0) {
            return false;
        }
        nap(5);
    }
    return false;
}

static char *mk(cJSON *m) { return proto_serialize(m); }

static void send_json(Transport *t, cJSON *m)
{
    char *s = mk(m);
    if (s) { transport_send(t, s); free(s); }
}

static void send_move(Transport *t, const char *uci, int ply)
{
    cJSON *m = proto_new(PROTO_C2S_MOVE);
    cJSON_AddStringToObject(m, "uci", uci);
    cJSON_AddNumberToObject(m, "ply", ply);
    send_json(t, m);
}

static void hello(Transport *t, const char *nick)
{
    cJSON *m = proto_new(PROTO_C2S_HELLO);
    cJSON_AddStringToObject(m, "nick", nick);
    send_json(t, m);
}

/* Read the next welcome and copy its session token. */
static bool welcome_token(Transport *t, char *out, size_t n)
{
    ProtoFrame f;
    memset(&f, 0, sizeof f);
    if (!expect(t, PROTO_S2C_WELCOME, 2000, &f)) return false;
    const char *tok = proto_field_str(&f, "token");
    bool ok = tok != NULL;
    if (tok) snprintf(out, n, "%s", tok);
    proto_frame_free(&f);
    return ok;
}

/* Every seat sees every authoritative move, so confirm both queues stay in sync. */
static void expect_move_both(Transport *t1, Transport *t2, const char *uci)
{
    ProtoFrame f;
    memset(&f, 0, sizeof f);
    CHECK(expect(t1, PROTO_S2C_MOVE, 2000, &f));
    if (f.type == PROTO_S2C_MOVE)
        CHECK(proto_field_str(&f, "uci") && strcmp(proto_field_str(&f, "uci"), uci) == 0);
    proto_frame_free(&f);
    CHECK(expect(t2, PROTO_S2C_MOVE, 2000, &f));
    if (f.type == PROTO_S2C_MOVE)
        CHECK(proto_field_str(&f, "uci") && strcmp(proto_field_str(&f, "uci"), uci) == 0);
    proto_frame_free(&f);
}

int main(void)
{
    if (!net_ws_available()) {
        printf("SKIP: built without libwebsockets\n");
        return 0;
    }

    const char *server = getenv("OPENCHESS_SERVER");
    if (!server || !*server) server = "./server/openchessd";

    int port = 42000 + (int)(getpid() % 10000);

    char dbpath[64];
    snprintf(dbpath, sizeof dbpath, "/tmp/oc_accounts_%d.db", (int)getpid());
    remove(dbpath);

    /* Start the server and wait for its "listening" line. */
    int fds[2];
    if (pipe(fds) != 0) { printf("SKIP: pipe failed\n"); return 0; }
    pid_t pid = fork();
    if (pid < 0) { printf("SKIP: fork failed\n"); return 0; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        setenv("OPENCHESS_QUIET", "1", 1);
        setenv("OPENCHESS_GRACE_MS", "1500", 1);   /* short grace for testing */
        setenv("OPENCHESS_ACCOUNTS_DB", dbpath, 1);
        char portstr[16];
        snprintf(portstr, sizeof portstr, "%d", port);
        execl(server, server, portstr, (char *)NULL);
        _exit(127);
    }
    close(fds[1]);

    char buf[128];
    ssize_t n = read(fds[0], buf, sizeof buf - 1);
    close(fds[0]);
    if (n <= 0) {
        printf("SKIP: server did not start (%s)\n", server);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        return 0;
    }
    buf[n] = '\0';

    char url[64];
    snprintf(url, sizeof url, "ws://127.0.0.1:%d/ws", port);

    Transport *a = NULL, *b = NULL, *d = NULL, *w2 = NULL, *b2 = NULL;
    Transport *c1 = NULL, *c2 = NULL;
    Transport *p1 = NULL, *p2 = NULL, *p2r = NULL;
    Transport *q1 = NULL, *q2 = NULL;
    Transport *s1 = NULL;
    Transport *u1 = NULL, *u2 = NULL;
    Transport *r1 = NULL, *r2 = NULL;

    /* --- two clients, hello/welcome --- */
    a = net_ws_connect(url);
    b = net_ws_connect(url);
    CHECK(a && b);
    if (!a || !b) goto cleanup;

    hello(a, "Alice");
    hello(b, "Bob");

    ProtoFrame f; memset(&f, 0, sizeof f);
    CHECK(expect(a, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(b, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);

    /* --- private room: create / join / start --- */
    send_json(a, proto_new(PROTO_C2S_CREATE));
    CHECK(expect(a, PROTO_S2C_ROOM, 2000, &f));
    char code[8] = "";
    const char *c = proto_field_str(&f, "code");
    if (c) snprintf(code, sizeof code, "%s", c);
    CHECK(code[0] != '\0');
    proto_frame_free(&f);

    cJSON *join = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(join, "code", code);
    send_json(b, join);

    CHECK(expect(b, PROTO_S2C_ROOM, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(a, PROTO_S2C_START, 2000, &f));
    CHECK(f.type == PROTO_S2C_START);
    proto_frame_free(&f);
    CHECK(expect(b, PROTO_S2C_START, 2000, &f));
    const char *bcol = proto_field_str(&f, "color");
    CHECK(bcol && strcmp(bcol, "black") == 0);
    proto_frame_free(&f);

    /* --- illegal move is rejected and resynced --- */
    send_move(a, "e2e5", 0);
    CHECK(expect(a, PROTO_S2C_REJECT, 2000, &f)); proto_frame_free(&f);

    /* --- authoritative move relay (both seats get the same echo) --- */
    send_move(a, "e2e4", 0);
    expect_move_both(a, b, "e2e4");
    send_move(b, "e7e5", 1);
    expect_move_both(a, b, "e7e5");
    /* out of turn: Black tries to move again and is rejected */
    send_move(b, "d7d5", 2);
    CHECK(expect(b, PROTO_S2C_REJECT, 2000, &f)); proto_frame_free(&f);

    /* --- resignation reaches both seats --- */
    send_json(a, proto_new(PROTO_C2S_RESIGN));
    CHECK(expect(a, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(b, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);

    /* --- matchmaking queue pairs two waiting players --- */
    d = net_ws_connect(url);
    CHECK(d != NULL);
    if (!d) goto cleanup;
    hello(d, "Dana");
    CHECK(expect(d, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);

    send_json(a, proto_new(PROTO_C2S_QUEUE));
    CHECK(expect(a, PROTO_S2C_QUEUED, 2000, &f)); proto_frame_free(&f);

    send_json(d, proto_new(PROTO_C2S_QUEUE));
    /* Dana was second: the pair starts immediately. Alice gets a start too. */
    bool a_start = expect(a, PROTO_S2C_START, 2000, &f);
    CHECK(a_start);
    if (a_start) proto_frame_free(&f);
    bool d_start = expect(d, PROTO_S2C_START, 2000, &f);
    CHECK(d_start);
    if (d_start) proto_frame_free(&f);

    /* --- authoritative checkmate (fool's mate) ends the game --- */
    w2 = net_ws_connect(url);
    b2 = net_ws_connect(url);
    CHECK(w2 && b2);
    if (!w2 || !b2) goto cleanup;
    hello(w2, "W");
    hello(b2, "B");
    CHECK(expect(w2, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(b2, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);

    send_json(w2, proto_new(PROTO_C2S_CREATE));
    CHECK(expect(w2, PROTO_S2C_ROOM, 2000, &f));
    char code2[8] = "";
    const char *fc2 = proto_field_str(&f, "code");
    if (fc2) snprintf(code2, sizeof code2, "%s", fc2);
    proto_frame_free(&f);

    cJSON *join2 = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(join2, "code", code2);
    send_json(b2, join2);
    CHECK(expect(b2, PROTO_S2C_ROOM, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(w2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(b2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);

    send_move(w2, "f2f3", 0); expect_move_both(w2, b2, "f2f3");
    send_move(b2, "e7e5", 1); expect_move_both(w2, b2, "e7e5");
    send_move(w2, "g2g4", 2); expect_move_both(w2, b2, "g2g4");
    send_move(b2, "d8h4", 3);

    CHECK(expect(w2, PROTO_S2C_GAMEOVER, 2000, &f));
    CHECK(strcmp(proto_field_str(&f, "result"), "0-1") == 0);
    CHECK(proto_field_str(&f, "reason") &&
          strcmp(proto_field_str(&f, "reason"), "checkmate") == 0);
    proto_frame_free(&f);
    CHECK(expect(b2, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);

    /* --- a timed game flags (1ms base) and ends on timeout --- */
    c1 = net_ws_connect(url);
    c2 = net_ws_connect(url);
    CHECK(c1 && c2);
    if (!c1 || !c2) goto cleanup;
    hello(c1, "T1"); hello(c2, "T2");
    CHECK(expect(c1, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(c2, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);

    cJSON *cr = proto_new(PROTO_C2S_CREATE);
    cJSON_AddNumberToObject(cr, "time", 1);
    send_json(c1, cr);
    CHECK(expect(c1, PROTO_S2C_ROOM, 2000, &f));
    char code3[8] = "";
    const char *fc3 = proto_field_str(&f, "code");
    if (fc3) snprintf(code3, sizeof code3, "%s", fc3);
    proto_frame_free(&f);

    cJSON *j3 = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(j3, "code", code3);
    send_json(c2, j3);
    CHECK(expect(c1, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(c2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);

    CHECK(expect(c1, PROTO_S2C_GAMEOVER, 3000, &f));
    CHECK(proto_field_str(&f, "reason") &&
          strcmp(proto_field_str(&f, "reason"), "timeout") == 0);
    proto_frame_free(&f);
    CHECK(expect(c2, PROTO_S2C_GAMEOVER, 3000, &f)); proto_frame_free(&f);

    /* --- presence, reconnect and rematch --- */
    p1 = net_ws_connect(url);
    p2 = net_ws_connect(url);
    CHECK(p1 && p2);
    if (!p1 || !p2) goto cleanup;
    hello(p1, "P1"); hello(p2, "P2");
    char tok1[40] = "", tok2[40] = "";
    CHECK(welcome_token(p1, tok1, sizeof tok1));
    CHECK(welcome_token(p2, tok2, sizeof tok2));
    CHECK(tok2[0] != '\0');

    send_json(p1, proto_new(PROTO_C2S_CREATE));
    CHECK(expect(p1, PROTO_S2C_ROOM, 2000, &f));
    char code4[8] = "";
    const char *fc4 = proto_field_str(&f, "code");
    if (fc4) snprintf(code4, sizeof code4, "%s", fc4);
    proto_frame_free(&f);
    cJSON *j4 = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(j4, "code", code4);
    send_json(p2, j4);
    CHECK(expect(p2, PROTO_S2C_ROOM, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(p1, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(p2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);

    /* P2 drops mid-game: P1 is told, and the seat is held for a grace period. */
    transport_close(p2);
    p2 = NULL;
    CHECK(expect(p1, PROTO_S2C_OPPONENT, 2000, &f));
    CHECK(proto_field_bool(&f, "connected", true) == false);
    proto_frame_free(&f);

    /* P2 reconnects with its token: gets the authoritative state and the seat. */
    p2r = net_ws_connect(url);
    CHECK(p2r != NULL);
    if (!p2r) goto cleanup;
    {
        cJSON *h = proto_new(PROTO_C2S_HELLO);
        cJSON_AddStringToObject(h, "nick", "P2");
        cJSON_AddStringToObject(h, "token", tok2);
        send_json(p2r, h);
    }
    CHECK(expect(p2r, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(p2r, PROTO_S2C_STATE, 2000, &f));
    CHECK(proto_field_str(&f, "fen") != NULL);
    proto_frame_free(&f);
    CHECK(expect(p1, PROTO_S2C_OPPONENT, 2000, &f));
    CHECK(proto_field_bool(&f, "connected", false) == true);
    proto_frame_free(&f);

    /* The resumed game continues normally. */
    send_move(p1, "e2e4", 0);
    expect_move_both(p1, p2r, "e2e4");

    /* Game over, then both agree to a rematch with swapped colours. */
    send_json(p1, proto_new(PROTO_C2S_RESIGN));
    CHECK(expect(p1, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(p2r, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);

    send_json(p1, proto_new(PROTO_C2S_REMATCH));
    CHECK(expect(p2r, PROTO_S2C_REMATCH, 2000, &f)); proto_frame_free(&f);
    send_json(p2r, proto_new(PROTO_C2S_REMATCH));
    CHECK(expect(p1, PROTO_S2C_START, 2000, &f));
    CHECK(proto_field_str(&f, "color") &&
          strcmp(proto_field_str(&f, "color"), "black") == 0);   /* swapped */
    proto_frame_free(&f);
    CHECK(expect(p2r, PROTO_S2C_START, 2000, &f));
    CHECK(proto_field_str(&f, "color") &&
          strcmp(proto_field_str(&f, "color"), "white") == 0);
    proto_frame_free(&f);

    /* --- spectator joins the running rematch (read-only) --- */
    s1 = net_ws_connect(url);
    CHECK(s1 != NULL);
    if (!s1) goto cleanup;
    hello(s1, "Spec");
    CHECK(expect(s1, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    {
        cJSON *sp = proto_new(PROTO_C2S_SPECTATE);
        cJSON_AddStringToObject(sp, "code", code4);
        send_json(s1, sp);
    }
    CHECK(expect(s1, PROTO_S2C_SPECTATE, 2000, &f));
    CHECK(proto_field_str(&f, "code") &&
          strcmp(proto_field_str(&f, "code"), code4) == 0);
    proto_frame_free(&f);

    /* P2R (White) moves; both the player and the spectator see it. */
    send_move(p2r, "e2e4", 0);
    CHECK(expect(p1, PROTO_S2C_MOVE, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(s1, PROTO_S2C_MOVE, 2000, &f));
    CHECK(strcmp(proto_field_str(&f, "uci"), "e2e4") == 0);
    proto_frame_free(&f);

    /* --- chat is relayed to the opponent (and spectators) --- */
    {
        cJSON *cm = proto_new(PROTO_C2S_CHAT);
        cJSON_AddStringToObject(cm, "text", "good luck");
        send_json(p1, cm);
    }
    CHECK(expect(p2r, PROTO_S2C_CHAT, 2000, &f));
    CHECK(proto_field_str(&f, "text") &&
          strcmp(proto_field_str(&f, "text"), "good luck") == 0);
    proto_frame_free(&f);

    /* --- draw: offer then accept ends the game 1/2-1/2 --- */
    send_json(p1, proto_new(PROTO_C2S_DRAW_OFFER));
    CHECK(expect(p2r, PROTO_S2C_DRAW, 2000, &f));
    CHECK(proto_field_str(&f, "action") &&
          strcmp(proto_field_str(&f, "action"), "offer") == 0);
    proto_frame_free(&f);
    send_json(p2r, proto_new(PROTO_C2S_DRAW_ACCEPT));
    CHECK(expect(p1, PROTO_S2C_GAMEOVER, 2000, &f));
    CHECK(proto_field_str(&f, "result") &&
          strcmp(proto_field_str(&f, "result"), "1/2-1/2") == 0);
    CHECK(proto_field_str(&f, "reason") &&
          strcmp(proto_field_str(&f, "reason"), "agreement") == 0);
    proto_frame_free(&f);
    CHECK(expect(p2r, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);

    /* --- abandonment: no reconnect within the grace period loses --- */
    q1 = net_ws_connect(url);
    q2 = net_ws_connect(url);
    CHECK(q1 && q2);
    if (!q1 || !q2) goto cleanup;
    hello(q1, "Q1"); hello(q2, "Q2");
    CHECK(expect(q1, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(q2, PROTO_S2C_WELCOME, 2000, &f)); proto_frame_free(&f);
    send_json(q1, proto_new(PROTO_C2S_CREATE));
    CHECK(expect(q1, PROTO_S2C_ROOM, 2000, &f));
    char code5[8] = "";
    const char *fc5 = proto_field_str(&f, "code");
    if (fc5) snprintf(code5, sizeof code5, "%s", fc5);
    proto_frame_free(&f);
    cJSON *j5 = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(j5, "code", code5);
    send_json(q2, j5);
    CHECK(expect(q1, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(q2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);

    transport_close(q2);
    q2 = NULL;
    CHECK(expect(q1, PROTO_S2C_OPPONENT, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(q1, PROTO_S2C_GAMEOVER, 4000, &f));
    CHECK(proto_field_str(&f, "reason") &&
          strcmp(proto_field_str(&f, "reason"), "abandoned") == 0);
    proto_frame_free(&f);

    /* --- accounts: register/login, puzzle rating, rated Elo --- */
    u1 = net_ws_connect(url);
    u2 = net_ws_connect(url);
    CHECK(u1 && u2);
    if (!u1 || !u2) goto cleanup;
    hello(u1, "alice");
    hello(u2, "bob");
    {
        char t[80];
        CHECK(welcome_token(u1, t, sizeof t));
        CHECK(welcome_token(u2, t, sizeof t));
    }

    /* register alice */
    {
        cJSON *m = proto_new(PROTO_C2S_REGISTER);
        cJSON_AddStringToObject(m, "user", "alice");
        cJSON_AddStringToObject(m, "pass", "pw1");
        send_json(u1, m);
    }
    bool has_accounts = false;
    CHECK(expect(u1, PROTO_S2C_AUTH, 2000, &f));
    has_accounts = proto_field_bool(&f, "ok", false);
    proto_frame_free(&f);

    if (has_accounts) {
        /* duplicate name rejected */
        {
            cJSON *m = proto_new(PROTO_C2S_REGISTER);
            cJSON_AddStringToObject(m, "user", "alice");
            cJSON_AddStringToObject(m, "pass", "pw1");
            send_json(u1, m);
        }
        CHECK(expect(u1, PROTO_S2C_AUTH, 2000, &f));
        CHECK(proto_field_bool(&f, "ok", true) == false);
        proto_frame_free(&f);

        /* wrong password rejected */
        {
            cJSON *m = proto_new(PROTO_C2S_LOGIN);
            cJSON_AddStringToObject(m, "user", "alice");
            cJSON_AddStringToObject(m, "pass", "nope");
            send_json(u2, m);
        }
        CHECK(expect(u2, PROTO_S2C_AUTH, 2000, &f));
        CHECK(proto_field_bool(&f, "ok", true) == false);
        proto_frame_free(&f);

        /* register bob (u2) */
        {
            cJSON *m = proto_new(PROTO_C2S_REGISTER);
            cJSON_AddStringToObject(m, "user", "bob");
            cJSON_AddStringToObject(m, "pass", "pw2");
            send_json(u2, m);
        }
        CHECK(expect(u2, PROTO_S2C_AUTH, 2000, &f));
        CHECK(proto_field_bool(&f, "ok", false) == true);
        proto_frame_free(&f);

        /* puzzle result raises the puzzle rating above 1500 */
        {
            cJSON *m = proto_new(PROTO_C2S_PUZZLE_RESULT);
            cJSON_AddNumberToObject(m, "rating", 1200);
            cJSON_AddBoolToObject(m, "solved", true);
            send_json(u1, m);
        }
        CHECK(expect(u1, PROTO_S2C_AUTH, 2000, &f));
        CHECK(proto_field_int(&f, "puzzle_rating", 0) > 1500);
        proto_frame_free(&f);

        /* rated game: alice (White) resigns, Elo moves both ways */
        {
            cJSON *m = proto_new(PROTO_C2S_CREATE);
            cJSON_AddBoolToObject(m, "rated", true);
            send_json(u1, m);
        }
        CHECK(expect(u1, PROTO_S2C_ROOM, 2000, &f));
        char rcode[8] = "";
        const char *rc = proto_field_str(&f, "code");
        if (rc) snprintf(rcode, sizeof rcode, "%s", rc);
        proto_frame_free(&f);
        {
            cJSON *m = proto_new(PROTO_C2S_JOIN);
            cJSON_AddStringToObject(m, "code", rcode);
            cJSON_AddBoolToObject(m, "rated", true);
            send_json(u2, m);
        }
        CHECK(expect(u1, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);
        CHECK(expect(u2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);

        send_json(u1, proto_new(PROTO_C2S_RESIGN));
        CHECK(expect(u1, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);
        CHECK(expect(u1, PROTO_S2C_AUTH, 2000, &f));
        CHECK(proto_field_int(&f, "pvp_rating", 1500) < 1500);   /* loser */
        proto_frame_free(&f);
        CHECK(expect(u2, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);
        CHECK(expect(u2, PROTO_S2C_AUTH, 2000, &f));
        CHECK(proto_field_int(&f, "pvp_rating", 1500) > 1500);   /* winner */
        proto_frame_free(&f);
        printf("accounts + rated Elo  ok\n");
    } else {
        printf("SKIP: accounts unavailable (no SQLite)\n");
    }

    /* --- threefold repetition ends the game as a draw --- */
    r1 = net_ws_connect(url);
    r2 = net_ws_connect(url);
    CHECK(r1 && r2);
    if (!r1 || !r2) goto cleanup;
    hello(r1, "R1");
    hello(r2, "R2");
    {
        char t[80];
        CHECK(welcome_token(r1, t, sizeof t));
        CHECK(welcome_token(r2, t, sizeof t));
    }
    send_json(r1, proto_new(PROTO_C2S_CREATE));
    CHECK(expect(r1, PROTO_S2C_ROOM, 2000, &f));
    char rcode[8] = "";
    const char *rc2 = proto_field_str(&f, "code");
    if (rc2) snprintf(rcode, sizeof rcode, "%s", rc2);
    proto_frame_free(&f);
    {
        cJSON *m = proto_new(PROTO_C2S_JOIN);
        cJSON_AddStringToObject(m, "code", rcode);
        send_json(r2, m);
    }
    CHECK(expect(r1, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);
    CHECK(expect(r2, PROTO_S2C_START, 2000, &f)); proto_frame_free(&f);

    /* Nf3 Nf6 Ng1 Ng8 twice returns to the start position a third time */
    const char *shuf[8] = { "g1f3","g8f6","f3g1","f6g8",
                            "g1f3","g8f6","f3g1","f6g8" };
    for (int i = 0; i < 8; i++) {
        Transport *mover = (i % 2 == 0) ? r1 : r2;
        send_move(mover, shuf[i], i);
        expect_move_both(r1, r2, shuf[i]);
    }
    CHECK(expect(r1, PROTO_S2C_GAMEOVER, 2000, &f));
    CHECK(proto_field_str(&f, "reason") &&
          strcmp(proto_field_str(&f, "reason"), "threefold repetition") == 0);
    CHECK(proto_field_str(&f, "result") &&
          strcmp(proto_field_str(&f, "result"), "1/2-1/2") == 0);
    proto_frame_free(&f);
    CHECK(expect(r2, PROTO_S2C_GAMEOVER, 2000, &f)); proto_frame_free(&f);
    printf("threefold repetition draw  ok\n");

cleanup:
    if (r2) transport_close(r2);
    if (r1) transport_close(r1);
    if (u2) transport_close(u2);
    if (u1) transport_close(u1);
    if (s1) transport_close(s1);
    if (q2) transport_close(q2);
    if (q1) transport_close(q1);
    if (p2r) transport_close(p2r);
    if (p2) transport_close(p2);
    if (p1) transport_close(p1);
    if (c2) transport_close(c2);
    if (c1) transport_close(c1);
    if (b2) transport_close(b2);
    if (w2) transport_close(w2);
    if (d) transport_close(d);
    if (b) transport_close(b);
    if (a) transport_close(a);
    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);
    remove(dbpath);

    if (failures == 0) {
        printf("online lobby + relay  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
