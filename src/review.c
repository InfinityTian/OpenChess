#include "review.h"
#include <math.h>

const char *review_glyph(ReviewClass c)
{
    switch (c) {
        case RC_BOOK:       return "B";
        case RC_FORCED:     return "=";
        case RC_BRILLIANT:  return "!!";
        case RC_GREAT:      return "!";
        case RC_BEST:       return "!";
        case RC_EXCELLENT:  return "";
        case RC_GOOD:       return "";
        case RC_INACCURACY: return "?!";
        case RC_MISTAKE:    return "?";
        case RC_BLUNDER:    return "??";
        case RC_MISS:       return "X";
        default:            return "";
    }
}

const char *review_name(ReviewClass c)
{
    switch (c) {
        case RC_BOOK:       return "Book";
        case RC_FORCED:     return "Forced";
        case RC_BRILLIANT:  return "Brilliant";
        case RC_GREAT:      return "Great";
        case RC_BEST:       return "Best";
        case RC_EXCELLENT:  return "Excellent";
        case RC_GOOD:       return "Good";
        case RC_INACCURACY: return "Inaccuracy";
        case RC_MISTAKE:    return "Mistake";
        case RC_BLUNDER:    return "Blunder";
        case RC_MISS:       return "Miss";
        default:            return "";
    }
}

double review_win_pct(int cp)
{
    double x = 0.00368208 * (double)cp;
    double w = 50.0 + 50.0 * (2.0 / (1.0 + exp(-x)) - 1.0);
    if (w < 0.0) w = 0.0;
    if (w > 100.0) w = 100.0;
    return w;
}

static double win_before(int cp, bool mate)
{
    if (mate) return cp > 0 ? 100.0 : 0.0;
    return review_win_pct(cp);
}

static double win_after(int cp, bool mate)
{
    if (mate) return cp > 0 ? 100.0 : 0.0;
    return review_win_pct(cp);
}

ReviewClass review_classify(int eval_before, bool mate_before,
                            int eval_after, bool mate_after,
                            int second_best, bool second_mate,
                            bool sacrifice, bool book, int legal_moves)
{
    if (legal_moves <= 1) return RC_FORCED;
    if (book) return RC_BOOK;

    double wb = win_before(eval_before, mate_before);
    double wa = win_after(eval_after, mate_after);
    double loss = wb - wa;
    if (loss < 0.0) loss = 0.0;

    /* Best move? (small loss) */
    bool best = (loss <= 1.0);

    /* Great: the only move that keeps the evaluation. */
    if (best && !mate_before) {
        double w2 = win_after(second_best, second_mate);
        if (wb - w2 >= 15.0 && wb >= 45.0) return RC_GREAT;
    }

    /* Brilliant: a sound sacrifice that is still best/good. */
    if (best && sacrifice && wa >= 45.0) return RC_BRILLIANT;

    /* Miss: a winning chance was available but not taken. */
    if (!best) {
        bool had_win = mate_before ? (eval_before > 0)
                                   : (wb >= 90.0 && eval_before >= 200);
        bool still_win = mate_after ? (eval_after > 0) : (wa >= 80.0);
        if (had_win && !still_win) return RC_MISS;
    }

    if (loss <= 1.0) return RC_BEST;
    if (loss < 2.0)  return RC_EXCELLENT;
    if (loss < 5.0)  return RC_GOOD;
    if (loss < 10.0) return RC_INACCURACY;
    if (loss < 20.0) return RC_MISTAKE;
    return RC_BLUNDER;
}
