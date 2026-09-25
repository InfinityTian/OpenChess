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

    char nm[96] = "";
    Board b2;
    board_reset(&b2);
    CHECK(opening_name_at(bk, &b2, nm, sizeof nm));
    play(&b2, "e4");
    CHECK(opening_name_at(bk, &b2, nm, sizeof nm));
    CHECK(strcmp(nm, "King's Pawn") == 0);
    play(&b2, "e5");
    CHECK(opening_name_at(bk, &b2, nm, sizeof nm));
    CHECK(strcmp(nm, "King's Pawn") == 0);
    play(&b2, "Nf3");
    CHECK(!opening_name_at(bk, &b2, nm, sizeof nm));

    Board c;
    board_reset(&c);
    play(&c, "d4");
    CHECK(opening_is_book(bk, &c));
    CHECK(opening_name_at(bk, &c, nm, sizeof nm));
    CHECK(strcmp(nm, "Test") == 0);

    /* position-based matching across transpositions */
    {
        const char *tp = "/tmp/oc_openings_transpose.tsv";
        FILE *tf = fopen(tp, "w");
        if (tf) {
            fputs("B00\tTrans A\t1. Nf3 d5 2. g3\n", tf);
            fputs("B01\tTrans B\t1. g3 d5 2. Nf3\n", tf);
            fclose(tf);
            OpeningBook *bk2 = opening_load(tp);
            CHECK(bk2 != NULL);
            Board t;
            board_reset(&t);
            play(&t, "Nf3");
            play(&t, "d5");
            play(&t, "g3");
            int idx[16];
            int cnt = opening_lines_at(bk2, &t, idx, 16);
            CHECK(cnt == 2);                   /* both orders match */
            char te[8] = "", tn[96] = "";
            CHECK(opening_pos_info(bk2, &t, te, sizeof te, tn, sizeof tn));
            CHECK(tn[0] != '\0');
            opening_free(bk2);
            remove(tp);
        }
    }

    /* stepping a line from a position (transposition-aware) */
    {
        Board s;
        board_reset(&s);
        char next[8] = "";
        CHECK(opening_line_next(bk, 0, &s, next, sizeof next) == 1);
        CHECK(strcmp(next, "e4") == 0);
        play(&s, "e4");
        CHECK(opening_line_next(bk, 0, &s, next, sizeof next) == 1);
        CHECK(strcmp(next, "e5") == 0);
    }

    /* path-based recognition (prefix matching, ECO + name) */
    {
        char p2[2][8] = { "e4", "e5" };
        char p1[1][8] = { "e4" };
        char p3[3][8] = { "e4", "e5", "Nf3" };
        char pb[1][8] = { "a4" };
        char eco[8] = "", nm2[96] = "";

        CHECK(opening_lookup(bk, (const char (*)[8])p2, 2, eco, sizeof eco,
                             nm2, sizeof nm2) == 2);
        CHECK(strcmp(eco, "C20") == 0);
        CHECK(strcmp(nm2, "King's Pawn") == 0);
        CHECK(opening_lookup(bk, (const char (*)[8])p1, 1, eco, sizeof eco,
                             nm2, sizeof nm2) == 1);
        CHECK(opening_lookup(bk, (const char (*)[8])p3, 3, eco, sizeof eco,
                             nm2, sizeof nm2) == 2);   /* book ran out at e5 */
        CHECK(opening_lookup(bk, (const char (*)[8])pb, 1, eco, sizeof eco,
                             nm2, sizeof nm2) == 0);
    }

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
