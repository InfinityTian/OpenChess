#ifndef FEN_H
#define FEN_H

#include "board.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Parse a Forsyth-Edwards Notation string into `out`.
 *
 * Piece placement, active color and castling rights are required; the
 * en-passant square, halfmove clock and fullmove number may be omitted and
 * default to "-", 0 and 1 respectively. Both kings must be present.
 *
 * Returns true on success (out fully populated); false leaves *out untouched.
 */
bool fen_parse(const char *fen, Board *out);

/*
 * Write the FEN for the given board into `out` (at most n bytes incl. NUL).
 */
void fen_generate(const Board *b, char *out, size_t n);

#endif
