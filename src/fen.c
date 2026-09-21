#include "fen.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **p)
{
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

bool fen_parse(const char *fen, Board *out)
{
    if (!fen || !out) return false;

    Board b;
    memset(&b, 0, sizeof b);
    b.side = WHITE;
    b.castling = 0;
    b.ep_square = -1;
    b.halfmove_clock = 0;
    b.fullmove_number = 1;

    const char *p = fen;
    skip_ws(&p);

    /* --- piece placement: 8 ranks, rank 8 first --- */
    int rank = 7, file = 0;
    for (; *p && *p != ' '; p++) {
        char c = *p;
        if (c == '/') {
            if (file != 8 || rank <= 0) return false;
            rank--;
            file = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) return false;
            continue;
        }
        Piece pc = char_to_piece(c);
        if (pc == EMPTY || file > 7) return false;
        b.board[rank * 8 + file] = pc;
        file++;
    }
    if (rank != 0 || file != 8) return false;

    /* --- active color --- */
    skip_ws(&p);
    if (*p != 'w' && *p != 'b') return false;
    b.side = (*p == 'w') ? WHITE : BLACK;
    p++;

    /* --- castling rights --- */
    skip_ws(&p);
    if (*p == '-') {
        p++;
    } else {
        if (*p == 0 || isspace((unsigned char)*p)) return false;
        for (; *p && !isspace((unsigned char)*p); p++) {
            switch (*p) {
                case 'K': b.castling |= 0x1; break;
                case 'Q': b.castling |= 0x2; break;
                case 'k': b.castling |= 0x4; break;
                case 'q': b.castling |= 0x8; break;
                default: return false;
            }
        }
    }

    /* --- en-passant target --- */
    skip_ws(&p);
    if (*p == '-') {
        p++;
    } else if (p[0] >= 'a' && p[0] <= 'h' && p[1] >= '1' && p[1] <= '8') {
        char alg[3] = { p[0], p[1], '\0' };
        b.ep_square = algebraic_to_sq(alg);
        p += 2;
    } else {
        return false;
    }

    /* --- halfmove clock (optional) --- */
    skip_ws(&p);
    if (*p) {
        if (!isdigit((unsigned char)*p)) return false;
        char *end;
        long hm = strtol(p, &end, 10);
        if (hm < 0 || hm > 999) return false;
        b.halfmove_clock = (int)hm;
        p = end;

        /* --- fullmove number (optional) --- */
        skip_ws(&p);
        if (*p) {
            if (!isdigit((unsigned char)*p)) return false;
            char *end2;
            long fm = strtol(p, &end2, 10);
            if (fm < 1 || fm > 9999) return false;
            b.fullmove_number = (uint8_t)(fm > 255 ? 255 : fm);
            p = end2;
        }
    }

    skip_ws(&p);
    if (*p != 0) return false;

    /* basic sanity: exactly one king per side */
    int wk = 0, bk = 0;
    for (int i = 0; i < 64; i++) {
        if (b.board[i] == WK) wk++;
        else if (b.board[i] == BK) bk++;
    }
    if (wk != 1 || bk != 1) return false;

    *out = b;
    return true;
}

void fen_generate(const Board *b, char *out, size_t n)
{
    if (!b || !out || n == 0) return;

    char buf[128];
    int k = 0;

    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            Piece p = b->board[rank * 8 + file];
            if (p == EMPTY) {
                empty++;
                continue;
            }
            if (empty) { buf[k++] = (char)('0' + empty); empty = 0; }
            buf[k++] = piece_to_char(p);
        }
        if (empty) buf[k++] = (char)('0' + empty);
        if (rank) buf[k++] = '/';
    }

    buf[k++] = ' ';
    buf[k++] = (b->side == WHITE) ? 'w' : 'b';
    buf[k++] = ' ';

    if (b->castling == 0) {
        buf[k++] = '-';
    } else {
        if (b->castling & 0x1) buf[k++] = 'K';
        if (b->castling & 0x2) buf[k++] = 'Q';
        if (b->castling & 0x4) buf[k++] = 'k';
        if (b->castling & 0x8) buf[k++] = 'q';
    }

    buf[k++] = ' ';
    if (b->ep_square < 0) {
        buf[k++] = '-';
    } else {
        char alg[3];
        sq_to_algebraic(b->ep_square, alg);
        buf[k++] = alg[0];
        buf[k++] = alg[1];
    }

    k += snprintf(buf + k, sizeof buf - (size_t)k, " %d %d",
                  b->halfmove_clock, b->fullmove_number);

    buf[k] = '\0';
    snprintf(out, n, "%s", buf);
}
