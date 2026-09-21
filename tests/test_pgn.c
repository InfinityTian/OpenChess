#include "../src/board.h"
#include "../src/move.h"
#include "../src/pgn.h"
#include "../src/fen.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool file_contains(FILE *f, const char *needle)
{
    rewind(f);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

static void test_write(void)
{
    const char *san[] = { "e4", "e5", "Nf3", "Nc6", "Bb5", "a6" };
    char moves[6][8];
    for (int i = 0; i < 6; i++) snprintf(moves[i], sizeof moves[i], "%s", san[i]);

    FILE *f = tmpfile();
    CHECK(f != NULL);
    if (!f) return;

    pgn_write(f, (const char (*)[8])moves, 6,
              "Test Event", "Test Site", "2026.01.01", 1,
              "Alice", "Bob", "*");

    CHECK(file_contains(f, "[Event \"Test Event\"]"));
    CHECK(file_contains(f, "[White \"Alice\"]"));
    CHECK(file_contains(f, "[Black \"Bob\"]"));
    CHECK(file_contains(f, "[Result \"*\"]"));
    CHECK(file_contains(f, "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 *"));
    fclose(f);
    printf("pgn_write headers + movetext  ok\n");
}

static void test_result(void)
{
    Board b;

    CHECK(fen_parse("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4", &b));
    CHECK(game_state(&b) == CHECKMATE);
    CHECK(strcmp(pgn_result(&b, CHECKMATE), "1-0") == 0);

    CHECK(fen_parse("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", &b));
    CHECK(game_state(&b) == STALEMATE);
    CHECK(strcmp(pgn_result(&b, STALEMATE), "1/2-1/2") == 0);

    CHECK(fen_parse("8/8/8/8/8/8/8/K6k w - - 0 1", &b));
    CHECK(strcmp(pgn_result(&b, NO_GAME_OVER), "*") == 0);
    printf("pgn_result tokens  ok\n");
}

int main(void)
{
    test_write();
    test_result();

    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
