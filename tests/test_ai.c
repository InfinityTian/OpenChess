#include "../src/ai.h"
#include "../src/fen.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void nap(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* Poll the engine until a bestmove arrives. Returns false on timeout. */
static bool await_move(AiEngine *ai, char out[8])
{
    for (int i = 0; i < 400; i++) {   /* up to ~4s */
        if (ai_poll_bestmove(ai, out)) return true;
        nap(10);
    }
    return false;
}

static void test_uci_to_move(void)
{
    Board b;
    board_reset(&b);
    Move m;
    CHECK(ai_uci_to_move(&b, "e2e4", &m));
    CHECK(MOVE_FROM(m) == algebraic_to_sq("e2"));
    CHECK(MOVE_TO(m) == algebraic_to_sq("e4"));
    CHECK(!ai_uci_to_move(&b, "e2e5", &m));      /* illegal */
    CHECK(!ai_uci_to_move(&b, "zzzz", &m));      /* malformed */

    Board p;
    CHECK(fen_parse("8/P7/8/8/8/8/8/K6k w - - 0 1", &p));
    CHECK(ai_uci_to_move(&p, "a7a8q", &m));
    CHECK((MOVE_FLAGS(m) & FLAG_PROMO) && MOVE_PROMO(m) == WQ);
    CHECK(ai_uci_to_move(&p, "a7a8n", &m));
    CHECK((MOVE_FLAGS(m) & FLAG_PROMO) && MOVE_PROMO(m) == WN);
    printf("uci -> move mapping  ok\n");
}

static void test_engine(AiEngine *ai)
{
    Board b;
    board_reset(&b);
    ai_set_movetime(ai, 150);
    ai_set_skill(ai, 20);
    ai_go(ai, &b);

    char uci[8] = {0};
    CHECK(await_move(ai, uci));
    Move m;
    CHECK(ai_uci_to_move(&b, uci, &m));
    printf("engine opening move = '%s'  ok\n", uci);

    /* mate in one: Ra8# */
    Board mb;
    CHECK(fen_parse("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", &mb));
    ai_go(ai, &mb);
    CHECK(await_move(ai, uci));
    Move mm;
    CHECK(ai_uci_to_move(&mb, uci, &mm));
    Board tmp = mb;
    make_move_plumb(&tmp, mm);
    CHECK(game_state(&tmp) == CHECKMATE);
    printf("engine finds mate in one ('%s')  ok\n", uci);
}

static void test_eval(AiEngine *ai)
{
    Board b;
    board_reset(&b);
    ai_go_infinite(ai, &b);

    bool got = false;
    int cp = 0, mate = 0, depth = 0;
    for (int i = 0; i < 400 && !got; i++) {
        char uci[8];
        ai_poll_bestmove(ai, uci);
        if (ai_get_eval(ai, &cp, &mate, &depth) && depth > 0) got = true;
        nap(10);
    }
    ai_stop_search(ai);
    CHECK(got);
    printf("engine eval: cp=%d mate=%d depth=%d  ok\n", cp, mate, depth);
}

int main(void)
{
    test_uci_to_move();

    const char *path = ai_find_engine(NULL);
    if (!path) {
        printf("\nSKIP: no stockfish binary found\n");
        return failures == 0 ? 0 : 1;
    }

    printf("using engine: %s\n", path);
    AiEngine *ai = ai_start(path);
    if (!ai) {
        fprintf(stderr, "FAIL: ai_start returned NULL\n");
        return 1;
    }
    CHECK(ai_alive(ai));
    test_engine(ai);
    test_eval(ai);
    ai_stop(ai);

    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
