#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_SIZE 8

typedef enum {
    EMPTY = 0,
    WP, WN, WB, WR, WQ, WK,
    BP, BN, BB, BR, BQ, BK,
} Piece;

#define WHITE_PIECE(p)  ((p) >= WP && (p) <= WK)
#define BLACK_PIECE(p)  ((p) >= BP && (p) <= BK)
#define IS_SLIDER(p)    ((p) == WB || (p) == WQ || (p) == BB || (p) == BQ || (p) == WR || (p) == BR)

typedef enum { WHITE, BLACK } Color;

typedef enum {
    NO_GAME_OVER,
    CHECKMATE,
    STALEMATE,
    INSUFFICIENT_MATERIAL,
    THREEFOLD_REPETITION,
    FIFTY_MOVE_RULE,
} GameState;

typedef struct {
    Piece board[64];
    Color side;
    uint8_t castling;        /* bit0: WK, bit1: WQ, bit2: BK, bit3: BQ */
    int ep_square;           /* -1 if none */
    int halfmove_clock;
    uint8_t fullmove_number;
} Board;

void  board_reset(Board *b);

Piece board_get(const Board *b, int sq);
void  board_set(Board *b, int sq, Piece p);
void  board_print(const Board *b);

int   algebraic_to_sq(const char *alg);      /* "e4" -> 28 */
void  sq_to_algebraic(int sq, char out[3]);
char  piece_to_char(Piece p);
Piece char_to_piece(char ch);

bool  sq_in_board(int sq);
int   square(int file, int rank);            /* file 0-7, rank 0-7; rank0=rank1 */

/* Repetition detection: two positions are "the same" when pieces, side to move,
 * castling rights and en-passant square match (the clocks are ignored). */
bool  board_rep_equal(const Board *a, const Board *b);
int   board_repetitions(const Board *cur, const Board *hist, int n);

#endif