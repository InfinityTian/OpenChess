#ifndef PGN_H
#define PGN_H

#include "board.h"
#include "move.h"
#include <stddef.h>

/*
 * Try to parse a SAN move (one move, e.g. "Nf3", "exd5", "O-O-O", "e8=Q+")
 * and, if legal, find the matching Move. The board is NOT modified.
 *
 * Returns: true if a legal move matched; the move is written to *out.
 */
bool san_find(const Board *b, const char *san, Move *out);

/*
 * Parse and apply a SAN move. Returns true on success (board modified);
 * false leaves the board unchanged.
 */
bool san_apply(Board *b, const char *san);

/*
 * Returns the resulting game state after a move WITHOUT applying it,
 * or -1 if the move is illegal. The board is not modified.
 */
int san_classify(const Board *b, const char *san);

/*
 * Convert a legal move into SAN given the board state BEFORE the move.
 * Writes at most n bytes including NUL into out.
 * `b` must be the board before the move is made.
 */
void move_to_san(const Board *b, Move m, char *out, size_t n);

#endif