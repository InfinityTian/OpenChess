#ifndef REVIEW_H
#define REVIEW_H

#include <stdbool.h>

/*
 * chess.com-style move classification from engine evaluations (centipawns,
 * side-to-move perspective; positive = good for the side that just moved).
 */

typedef enum {
    RC_NONE = 0,
    RC_BOOK,
    RC_FORCED,
    RC_BRILLIANT,
    RC_GREAT,
    RC_BEST,
    RC_EXCELLENT,
    RC_GOOD,
    RC_INACCURACY,
    RC_MISTAKE,
    RC_BLUNDER,
    RC_MISS,
} ReviewClass;

const char *review_glyph(ReviewClass c);   /* "!!", "!", "?!", "?", "??", "X", ... */
const char *review_name(ReviewClass c);    /* "Brilliant", "Blunder", ... */

/* Board badge glyph: like review_glyph but "best" shows a star instead of
 * nothing. Returns NULL when the class has no badge. */
const char *review_badge_glyph(ReviewClass c);

/* Win percentage (0..100) for a side-to-move centipawn score. */
double review_win_pct(int cp);

/* Per-move accuracy (0..100) from the win% before/after the move (mover's
 * point of view), using the Lichess formula. */
double review_move_accuracy(double win_before, double win_after);

/*
 * Classify a move. `eval_before`/`second_best` are the best and second-best
 * scores in the position before the move; `eval_after` is the score of the
 * position after the move, negated to the mover's perspective. `sacrifice`
 * means the played line gives up material. `book` when the position is in the
 * opening book. `legal_moves` is the number of legal moves before.
 */
ReviewClass review_classify(int eval_before, bool mate_before,
                            int eval_after, bool mate_after,
                            int second_best, bool second_mate,
                            bool sacrifice, bool book, int legal_moves);

#endif
