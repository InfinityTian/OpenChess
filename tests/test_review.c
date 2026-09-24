#include "../src/review.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static ReviewClass cls(int eb, bool mb, int ea, bool ma, int sb, bool sm,
                       bool sac, bool book, int legal)
{
    return review_classify(eb, mb, ea, ma, sb, sm, sac, book, legal);
}

int main(void)
{
    /* forced and book take priority */
    CHECK(cls(0, false, 0, false, 0, false, false, false, 1) == RC_FORCED);
    CHECK(cls(0, false, 0, false, 0, false, false, true, 20) == RC_BOOK);

    /* best move, no loss */
    CHECK(cls(50, false, 50, false, 50, false, false, false, 30) == RC_BEST);

    /* great: only move (second best collapses) */
    CHECK(cls(50, false, 50, false, -400, false, false, false, 30) == RC_GREAT);

    /* brilliant: sacrifice, still best and good */
    CHECK(cls(40, false, 30, false, 30, false, true, false, 30) == RC_BRILLIANT);

    /* missed win: had mate, played a non-best move that keeps only a small plus */
    CHECK(cls(600, false, 50, false, 600, false, false, false, 30) == RC_MISS);

    /* bands by win% drop */
    CHECK(cls(0, false, -20, false, -400, false, false, false, 30) == RC_EXCELLENT);
    CHECK(cls(0, false, -50, false, -400, false, false, false, 30) == RC_GOOD);
    CHECK(cls(0, false, -80, false, -500, false, false, false, 30) == RC_INACCURACY);
    CHECK(cls(0, false, -150, false, -600, false, false, false, 30) == RC_MISTAKE);
    CHECK(cls(0, false, -400, false, -1000, false, false, false, 30) == RC_BLUNDER);

    /* win% sanity */
    CHECK(review_win_pct(0) > 49.9 && review_win_pct(0) < 50.1);
    CHECK(review_win_pct(1000) > 90.0);
    CHECK(review_win_pct(-1000) < 10.0);

    CHECK(strcmp(review_glyph(RC_BLUNDER), "??") == 0);
    CHECK(strcmp(review_name(RC_BRILLIANT), "Brilliant") == 0);

    if (failures == 0) {
        printf("review classification  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
