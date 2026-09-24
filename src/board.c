#include "board.h"
#include <stdio.h>
#include <string.h>

void board_reset(Board *b)
{
    static const char *start =
        "RNBQKBNR"
        "PPPPPPPP"
        "........"
        "........"
        "........"
        "........"
        "pppppppp"
        "rnbqkbnr";

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            b->board[rank * 8 + file] = char_to_piece(start[rank * 8 + file]);
        }
    }
    b->side = WHITE;
    b->castling = 0xF;
    b->ep_square = -1;
    b->halfmove_clock = 0;
    b->fullmove_number = 1;
}

Piece board_get(const Board *b, int sq)
{
    if (sq < 0 || sq >= 64) return EMPTY;
    return b->board[sq];
}

void board_set(Board *b, int sq, Piece p)
{
    if (sq < 0 || sq >= 64) return;
    b->board[sq] = p;
}

int square(int file, int rank)
{
    return rank * 8 + file;
}

bool sq_in_board(int sq)
{
    return sq >= 0 && sq < 64;
}

int algebraic_to_sq(const char *alg)
{
    if (alg[0] < 'a' || alg[0] > 'h') return -1;
    if (alg[1] < '1' || alg[1] > '8') return -1;
    int file = alg[0] - 'a';
    int rank = alg[1] - '1';
    return square(file, rank);
}

void sq_to_algebraic(int sq, char out[3])
{
    out[0] = (char)('a' + (sq % 8));
    out[1] = (char)('1' + (sq / 8));
    out[2] = '\0';
}

void board_print(const Board *b)
{
    for (int rank = 7; rank >= 0; rank--) {
        for (int file = 0; file < 8; file++) {
            char ch = '.';
            switch (b->board[rank * 8 + file]) {
                case WP: ch = 'P'; break;
                case WN: ch = 'N'; break;
                case WB: ch = 'B'; break;
                case WR: ch = 'R'; break;
                case WQ: ch = 'Q'; break;
                case WK: ch = 'K'; break;
                case BP: ch = 'p'; break;
                case BN: ch = 'n'; break;
                case BB: ch = 'b'; break;
                case BR: ch = 'r'; break;
                case BQ: ch = 'q'; break;
                case BK: ch = 'k'; break;
                default: ch = '.'; break;
            }
            printf("%c ", ch);
        }
        printf("\n");
    }
}

char piece_to_char(Piece p)
{
    switch (p) {
        case WP: return 'P';
        case WN: return 'N';
        case WB: return 'B';
        case WR: return 'R';
        case WQ: return 'Q';
        case WK: return 'K';
        case BP: return 'p';
        case BN: return 'n';
        case BB: return 'b';
        case BR: return 'r';
        case BQ: return 'q';
        case BK: return 'k';
        default: return '.';
    }
}

Piece char_to_piece(char ch)
{
    switch (ch) {
        case 'P': return WP;
        case 'N': return WN;
        case 'B': return WB;
        case 'R': return WR;
        case 'Q': return WQ;
        case 'K': return WK;
        case 'p': return BP;
        case 'n': return BN;
        case 'b': return BB;
        case 'r': return BR;
        case 'q': return BQ;
        case 'k': return BK;
        default: return EMPTY;
    }
}
bool board_rep_equal(const Board *a, const Board *b)
{
    if (!a || !b) return false;
    if (a->side != b->side) return false;
    if (a->castling != b->castling) return false;
    if (a->ep_square != b->ep_square) return false;
    for (int i = 0; i < 64; i++)
        if (a->board[i] != b->board[i]) return false;
    return true;
}

int board_repetitions(const Board *cur, const Board *hist, int n)
{
    if (!cur || !hist || n <= 0) return 0;
    int count = 0;
    for (int i = 0; i < n; i++)
        if (board_rep_equal(cur, &hist[i])) count++;
    return count;
}
