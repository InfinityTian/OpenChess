#include "../src/opening.h"
#include "../src/board.h"
#include "../src/move.h"
#include "../src/pgn.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void play(Board *b, const char *san)
{
    Move m;
    if (san_find(b, san, &m)) make_move_plumb(b, m);
}

int main(void)
{
    const char *path = "/tmp/oc_openings_test.tsv";
    FILE *f = fopen(path, "w");
    if (!f) { printf("SKIP: cannot write\n"); return 0; }
    fputs("C20\tKing's Pawn\t1. e4 e5\n", f);
    fputs("A00\tTest\t1. d4 d5\n", f);
    fclose(f);

    OpeningBook *bk = opening_load(path);
    CHECK(bk != NULL);
    if (!bk) { remove(path); return 1; }

    Board b;
    board_reset(&b);
    CHECK(opening_is_book(bk, &b));       /* start position is on every line */

    play(&b, "e4");
    CHECK(opening_is_book(bk, &b));
    play(&b, "e5");
    CHECK(opening_is_book(bk, &b));
    play(&b, "Nf3");
    CHECK(!opening_is_book(bk, &b));      /* off book */

    Board c;
    board_reset(&c);
    play(&c, "d4");
    CHECK(opening_is_book(bk, &c));

    opening_free(bk);
    remove(path);
    CHECK(opening_load("/tmp/oc_openings_missing.tsv") == NULL);

    if (failures == 0) {
        printf("opening book  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
