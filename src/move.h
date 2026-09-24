#ifndef MOVE_H
#define MOVE_H

#include "board.h"
#include <stdbool.h>

#define FLAG_PROMO      0x1000
#define FLAG_EP         0x2000
#define FLAG_CASTLE_K   0x4000
#define FLAG_CASTLE_Q   0x8000
#define FLAG_CAPTURE    0x10000
#define FLAG_MASK       0x1F000

/* encoded move: from | (to<<6) | flags | (promo<<20) */
typedef uint32_t Move;

#define MOVE_FROM(m)  ((int)((m) & 0x3F))
#define MOVE_TO(m)    ((int)(((m) >> 6) & 0x3F))
#define MOVE_FLAGS(m) ((m) & FLAG_MASK)
#define MOVE_PROMO(m) ((Piece)(((m) >> 20) & 0x7))

static inline Move make_move(int from, int to, uint32_t flags, Piece promo)
{
    return (Move)from | ((Move)to << 6) | flags | ((Move)(promo & 0x7) << 20);
}

#define MAX_MOVES 256
typedef struct {
    Move moves[MAX_MOVES];
    int count;
} MoveList;

void   gen_pseudo(const Board *b, MoveList *ml);
void   gen_legal(const Board *b, MoveList *ml);
bool   in_check(const Board *b, Color side);
bool   is_legal(const Board *b, Move m);
GameState game_state(const Board *b);
int    find_king(const Board *b, Color side);
void   make_move_plumb(Board *b, Move m);       /* apply without legality check */
bool   square_attacked(const Board *b, int sq, Color by_side);

/* Resolve a UCI string ("e2e4", "e7e8q") against the legal moves of `b`.
 * Writes the matching Move to *out; false if there is no such legal move. */
bool   uci_to_move(const Board *b, const char *uci, Move *out);

#endif