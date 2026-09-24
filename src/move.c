#include "move.h"
#include <string.h>

static const int dirs_knight[8][2] = {{2,1},{2,-1},{-2,1},{-2,-1},{1,2},{1,-2},{-1,2},{-1,-2}};
static const int dirs_king[8][2]   = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
static const int dirs_bishop[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
static const int dirs_rook[4][2]   = {{1,0},{-1,0},{0,1},{0,-1}};

static inline int p_file(int sq) { return sq % 8; }
static inline int p_rank(int sq) { return sq / 8; }

int find_king(const Board *b, Color side)
{
    Piece k = (side == WHITE) ? WK : BK;
    for (int i = 0; i < 64; i++)
        if (b->board[i] == k) return i;
    return -1;
}

bool square_attacked(const Board *b, int sq, Color by_side)
{
    int f = p_file(sq), r = p_rank(sq);

    Piece pawn = (by_side == WHITE) ? WP : BP;
    int pdir = (by_side == WHITE) ? 1 : -1;
    for (int df = -1; df <= 1; df += 2) {
        int nf = f + df, nr = r - pdir;
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
            if (b->board[nr * 8 + nf] == pawn) return true;
        }
    }

    Piece knight = (by_side == WHITE) ? WN : BN;
    for (int i = 0; i < 8; i++) {
        int nf = f + dirs_knight[i][0], nr = r + dirs_knight[i][1];
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8 && b->board[nr * 8 + nf] == knight)
            return true;
    }

    Piece king = (by_side == WHITE) ? WK : BK;
    for (int i = 0; i < 8; i++) {
        int nf = f + dirs_king[i][0], nr = r + dirs_king[i][1];
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8 && b->board[nr * 8 + nf] == king)
            return true;
    }

    Piece rook = (by_side == WHITE) ? WR : BR;
    Piece bishop = (by_side == WHITE) ? WB : BB;
    Piece queen = (by_side == WHITE) ? WQ : BQ;

    for (int i = 0; i < 4; i++) {
        int nf = f, nr = r;
        while (1) {
            nf += dirs_rook[i][0]; nr += dirs_rook[i][1];
            if (nf < 0 || nf >= 8 || nr < 0 || nr >= 8) break;
            Piece p = b->board[nr * 8 + nf];
            if (p == EMPTY) continue;
            if (p == rook || p == queen) return true;
            break;
        }
    }
    for (int i = 0; i < 4; i++) {
        int nf = f, nr = r;
        while (1) {
            nf += dirs_bishop[i][0]; nr += dirs_bishop[i][1];
            if (nf < 0 || nf >= 8 || nr < 0 || nr >= 8) break;
            Piece p = b->board[nr * 8 + nf];
            if (p == EMPTY) continue;
            if (p == bishop || p == queen) return true;
            break;
        }
    }
    return false;
}

bool in_check(const Board *b, Color side)
{
    int k = find_king(b, side);
    if (k < 0) return false;
    Color opp = (side == WHITE) ? BLACK : WHITE;
    return square_attacked(b, k, opp);
}

static void add_move(MoveList *ml, int from, int to, uint32_t flags)
{
    ml->moves[ml->count++] = make_move(from, to, flags, WQ);
}

static void add_promos(MoveList *ml, int from, int to, uint32_t base)
{
    for (Piece pr = WN; pr <= WQ; pr++)
        ml->moves[ml->count++] = make_move(from, to, base | FLAG_PROMO, pr);
}

void gen_pseudo(const Board *b, MoveList *ml)
{
    ml->count = 0;
    Color s = b->side;
    bool is_white = (s == WHITE);

    for (int from = 0; from < 64; from++) {
        Piece p = b->board[from];
        if (p == EMPTY) continue;
        if (is_white != WHITE_PIECE(p)) continue;

        int f = p_file(from), r = p_rank(from);

        switch (p) {
            case WN: case BN:
                for (int i = 0; i < 8; i++) {
                    int nf = f + dirs_knight[i][0], nr = r + dirs_knight[i][1];
                    if (nf < 0 || nf >= 8 || nr < 0 || nr >= 8) continue;
                    int to = nr * 8 + nf;
                    Piece t = b->board[to];
                    if (t == EMPTY) add_move(ml, from, to, 0);
                    else if (is_white != WHITE_PIECE(t)) add_move(ml, from, to, FLAG_CAPTURE);
                }
                break;

            case WK: case BK: {
                for (int i = 0; i < 8; i++) {
                    int nf = f + dirs_king[i][0], nr = r + dirs_king[i][1];
                    if (nf < 0 || nf >= 8 || nr < 0 || nr >= 8) continue;
                    int to = nr * 8 + nf;
                    Piece t = b->board[to];
                    if (t == EMPTY || is_white != WHITE_PIECE(t))
                        add_move(ml, from, to, t == EMPTY ? 0 : FLAG_CAPTURE);
                }
                if (p == WK) {
                    if ((b->castling & 0x1) && b->board[5] == EMPTY && b->board[6] == EMPTY &&
                        b->board[7] == WR &&
                        !square_attacked(b, 4, BLACK) && !square_attacked(b, 5, BLACK) &&
                        !square_attacked(b, 6, BLACK))
                        add_move(ml, from, 6, FLAG_CASTLE_K);
                    if ((b->castling & 0x2) && b->board[1] == EMPTY && b->board[2] == EMPTY &&
                        b->board[3] == EMPTY && b->board[0] == WR &&
                        !square_attacked(b, 4, BLACK) && !square_attacked(b, 3, BLACK) &&
                        !square_attacked(b, 2, BLACK))
                        add_move(ml, from, 2, FLAG_CASTLE_Q);
                } else {
                    if ((b->castling & 0x4) && b->board[61] == EMPTY && b->board[62] == EMPTY &&
                        b->board[63] == BR &&
                        !square_attacked(b, 60, WHITE) && !square_attacked(b, 61, WHITE) &&
                        !square_attacked(b, 62, WHITE))
                        add_move(ml, from, 62, FLAG_CASTLE_K);
                    if ((b->castling & 0x8) && b->board[57] == EMPTY && b->board[58] == EMPTY &&
                        b->board[59] == EMPTY && b->board[56] == BR &&
                        !square_attacked(b, 60, WHITE) && !square_attacked(b, 59, WHITE) &&
                        !square_attacked(b, 58, WHITE))
                        add_move(ml, from, 58, FLAG_CASTLE_Q);
                }
                break;
            }

            case WQ: case BQ:
            case WB: case BB:
            case WR: case BR: {
                bool diag = (p == WQ || p == BQ || p == WB || p == BB);
                bool orth = (p == WQ || p == BQ || p == WR || p == BR);

                if (diag) {
                    for (int d = 0; d < 4; d++) {
                        int nf = f, nr = r;
                        while (1) {
                            nf += dirs_bishop[d][0]; nr += dirs_bishop[d][1];
                            if (nf < 0 || nf >= 8 || nr < 0 || nr >= 8) break;
                            int to = nr * 8 + nf;
                            Piece t = b->board[to];
                            if (t == EMPTY) add_move(ml, from, to, 0);
                            else { if (is_white != WHITE_PIECE(t)) add_move(ml, from, to, FLAG_CAPTURE); break; }
                        }
                    }
                }
                if (orth) {
                    for (int d = 0; d < 4; d++) {
                        int nf = f, nr = r;
                        while (1) {
                            nf += dirs_rook[d][0]; nr += dirs_rook[d][1];
                            if (nf < 0 || nf >= 8 || nr < 0 || nr >= 8) break;
                            int to = nr * 8 + nf;
                            Piece t = b->board[to];
                            if (t == EMPTY) add_move(ml, from, to, 0);
                            else { if (is_white != WHITE_PIECE(t)) add_move(ml, from, to, FLAG_CAPTURE); break; }
                        }
                    }
                }
                break;
            }

            case WP: case BP: {
                int dir = (p == WP) ? 1 : -1;
                int start_rank = (p == WP) ? 1 : 6;
                int promo_rank  = (p == WP) ? 7 : 0;

                int nr = r + dir;
                if (nr >= 0 && nr < 8 && b->board[nr * 8 + f] == EMPTY) {
                    int to = nr * 8 + f;
                    if (nr == promo_rank) add_promos(ml, from, to, 0);
                    else {
                        add_move(ml, from, to, 0);
                        if (r == start_rank) {
                            int nr2 = r + 2 * dir;
                            if (b->board[nr2 * 8 + f] == EMPTY)
                                add_move(ml, from, nr2 * 8 + f, 0);
                        }
                    }
                }
                for (int df = -1; df <= 1; df += 2) {
                    int nf = f + df;
                    if (nf < 0 || nf >= 8) continue;
                    int nr2 = r + dir;
                    if (nr2 < 0 || nr2 >= 8) continue;
                    int to = nr2 * 8 + nf;
                    Piece t = b->board[to];
                    if (t != EMPTY && is_white != WHITE_PIECE(t)) {
                        if (nr2 == promo_rank) add_promos(ml, from, to, FLAG_CAPTURE);
                        else add_move(ml, from, to, FLAG_CAPTURE);
                    }
                    if (to == b->ep_square && t == EMPTY) {
                        add_move(ml, from, to, FLAG_EP);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void make_move_plumb(Board *b, Move m)
{
    int from = MOVE_FROM(m), to = MOVE_TO(m);
    uint32_t flags = MOVE_FLAGS(m);
    Piece p = b->board[from];

    b->board[from] = EMPTY;

    if (flags & FLAG_EP) {
        b->board[(to < from) ? (to + 8) : (to - 8)] = EMPTY;
    } else if (flags & FLAG_CASTLE_K) {
        if (to == 6) { b->board[7] = EMPTY; b->board[5] = WR; }
        else if (to == 62) { b->board[63] = EMPTY; b->board[61] = BR; }
    } else if (flags & FLAG_CASTLE_Q) {
        if (to == 2) { b->board[0] = EMPTY; b->board[3] = WR; }
        else if (to == 58) { b->board[56] = EMPTY; b->board[59] = BR; }
    }

    b->board[to] = p;

    if (flags & FLAG_PROMO) {
        Piece pr = MOVE_PROMO(m);
        b->board[to] = WHITE_PIECE(p) ? pr : (Piece)(pr + (BP - WN));
    }

    b->ep_square = -1;
    if ((p == WP || p == BP) && (to == from + 16 || to == from - 16))
        b->ep_square = (from + to) / 2;

    if (p == WK) b->castling &= ~0x3;
    if (p == BK) b->castling &= ~0xC;
    if (from == 7 || to == 7) b->castling &= ~0x1;
    if (from == 0 || to == 0) b->castling &= ~0x2;
    if (from == 63 || to == 63) b->castling &= ~0x4;
    if (from == 56 || to == 56) b->castling &= ~0x8;

    if (p == WP || p == BP || (flags & FLAG_CAPTURE))
        b->halfmove_clock = 0;
    else
        b->halfmove_clock++;

    if (b->side == BLACK) b->fullmove_number++;
    b->side = (b->side == WHITE) ? BLACK : WHITE;
}

bool is_legal(const Board *b, Move m)
{
    Board tmp = *b;
    make_move_plumb(&tmp, m);
    return !in_check(&tmp, b->side);
}

void gen_legal(const Board *b, MoveList *ml)
{
    MoveList pseudo;
    gen_pseudo(b, &pseudo);
    ml->count = 0;
    for (int i = 0; i < pseudo.count; i++) {
        if (is_legal(b, pseudo.moves[i]))
            ml->moves[ml->count++] = pseudo.moves[i];
    }
}

GameState game_state(const Board *b)
{
    MoveList legal;
    gen_legal(b, &legal);

    if (legal.count == 0) {
        if (in_check(b, b->side)) return CHECKMATE;
        return STALEMATE;
    }

    int pieces = 0, minors = 0;
    for (int i = 0; i < 64; i++) {
        Piece p = b->board[i];
        if (p == EMPTY) continue;
        pieces++;
        if (p == WN || p == BN || p == WB || p == BB) minors++;
        if (p == WQ || p == BQ || p == WR || p == BR || p == WP || p == BP)
            return NO_GAME_OVER;
    }
    if (pieces <= 2 || (pieces <= 3 && minors == 1))
        return INSUFFICIENT_MATERIAL;
    return NO_GAME_OVER;
}

bool uci_to_move(const Board *b, const char *uci, Move *out)
{
    if (!b || !uci || !out || strlen(uci) < 4) return false;

    char a[3] = { uci[0], uci[1], 0 };
    char c[3] = { uci[2], uci[3], 0 };
    if (a[0] < 'a' || a[0] > 'h' || a[1] < '1' || a[1] > '8') return false;
    if (c[0] < 'a' || c[0] > 'h' || c[1] < '1' || c[1] > '8') return false;
    int from = algebraic_to_sq(a), to = algebraic_to_sq(c);

    MoveList legal;
    gen_legal(b, &legal);
    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        if (MOVE_FROM(m) != from || MOVE_TO(m) != to) continue;
        if (MOVE_FLAGS(m) & FLAG_PROMO) {
            if (strlen(uci) < 5) continue;
            char pc = uci[4];
            Piece want = (pc == 'r') ? WR : (pc == 'b') ? WB : (pc == 'n') ? WN : WQ;
            Piece got = MOVE_PROMO(m);
            if (WHITE_PIECE(got)) { if (got != want) continue; }
            else                  { if (got != (Piece)(want + (BP - WN))) continue; }
        }
        *out = m;
        return true;
    }
    return false;
}