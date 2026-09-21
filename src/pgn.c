#include "pgn.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Piece letter -> piece for the side given (true=white). */
static Piece letter_to_piece(char c, bool white)
{
    char u = (char)toupper(c);
    switch (u) {
        case 'K': return white ? WK : BK;
        case 'N': return white ? WN : BN;
        case 'B': return white ? WB : BB;
        case 'R': return white ? WR : BR;
        case 'Q': return white ? WQ : BQ;
        default:  return EMPTY;
    }
}

/* Default promo piece (queen) for the given side. */
static Piece default_promo(bool white)
{
    return white ? WQ : BQ;
}

/*
 * Parse and find a single SAN move. Grammar:
 *   [pieceletter][disamb-file][disamb-rank][x]dest[=PR][+][#]
 *   O-O, O-O-O, and 0-0/0-0-0 accepted
 */
bool san_find(const Board *b, const char *san, Move *out)
{
    char s[16];
    size_t len = strlen(san);
    if (len == 0 || len >= sizeof(s)) return false;
    memcpy(s, san, len + 1);

    /* trim whitespace */
    size_t start = 0, end = len;
    while (start < end && isspace((unsigned char)s[start])) start++;

    /* normalize 0 (zero) to O for castling */
    for (size_t i = start; i < end; i++)
        if (s[i] == '0') s[i] = 'O';

    bool white = (b->side == WHITE);
    Piece moved_piece = EMPTY;
    int df = -1, dr = -1;   /* disambiguation file/rank */
    Piece promo = default_promo(white);

    /* --- castling --- */
    char stripped[16];
    int k = 0;
    for (size_t i = start; i < end; i++) {
        char c = s[i];
        if (c == '+' || c == '#') continue;   /* ignore check/mate suffix for matching */
        if (c == 'O' || c == '-' || (c >= '0' && c <= '9')) stripped[k++] = c;
    }
    stripped[k] = '\0';

    if (strcmp(stripped, "O-O") == 0 || strcmp(stripped, "OO") == 0 ||
        strcmp(stripped, "O-O-O") == 0 || strcmp(stripped, "OOO") == 0) {
        bool queenside = (strstr(stripped, "O-O-O") || strstr(stripped, "OOO"));
        /* find the legal castle move */
        MoveList ml;
        gen_legal(b, &ml);
        for (int i = 0; i < ml.count; i++) {
            Move m = ml.moves[i];
            uint32_t fl = MOVE_FLAGS(m);
            if (queenside && (fl & FLAG_CASTLE_Q)) { *out = m; return true; }
            if (!queenside && (fl & FLAG_CASTLE_K)) { *out = m; return true; }
        }
        return false;
    }

    /* --- normal move --- */
    /* find promotion marker */
    {
        const char *eq = strchr(s + start, '=');
        if (eq) {
            char c = eq[1];
            Piece pr = letter_to_piece(c, white);
            if (pr == EMPTY) return false;
            promo = pr;
        }
    }

    /* determine destination: last 'a-h' followed by a digit */
    int dst = -1;
    int dest_idx = -1;     /* index of destination file char */
    for (int i = (int)end - 1; i >= (int)start; i--) {
        if (isdigit((unsigned char)s[i])) {
            if (i >= (int)start + 1 && s[i-1] >= 'a' && s[i-1] <= 'h') {
                char alg[3] = { s[i-1], s[i], '\0' };
                dst = algebraic_to_sq(alg);
                dest_idx = i - 1;
                break;
            }
        }
    }
    if (dst < 0) return false;

    /* piece letter = first char (either N/B/R/Q/K or a pawn file) */
    char c0 = s[start];
    if (c0 >= 'A' && c0 <= 'Z') {
        moved_piece = letter_to_piece(c0, white);
        if (moved_piece == EMPTY) return false;
    } else {
        /* pawn move: c0 is the origin file (a-h) */
        moved_piece = white ? WP : BP;
        if (c0 >= 'a' && c0 <= 'h') df = c0 - 'a';
    }

    /* disambiguation: scan chars between the piece letter and the destination
       for an origin file (a-h) or origin rank (1-8). */
    size_t dstart = start;
    if (c0 >= 'A' && c0 <= 'Z') dstart += 1;
    for (size_t j = dstart; (int)j < dest_idx; j++) {
        char c = s[j];
        if (c == 'x' || c == '=' || c == '+' || c == '#' || c == ' ' || c == '.') continue;
        if (c >= 'a' && c <= 'h') { if (df < 0) df = c - 'a'; }
        else if (c >= '1' && c <= '8') { if (dr < 0) dr = c - '1'; }
    }

    /* match against legal moves */
    /* stored promo pieces are always white minors (WN..WQ); normalize */
    Piece promo_w = white ? promo : (Piece)(promo - (BP - WN));
    MoveList ml;
    gen_legal(b, &ml);
    for (int i = 0; i < ml.count; i++) {
        Move m = ml.moves[i];
        int from = MOVE_FROM(m), to = MOVE_TO(m);
        if (to != dst) continue;
        uint32_t fl = MOVE_FLAGS(m);
        Piece mp = b->board[from];
        if (mp != moved_piece) continue;
        if ((fl & FLAG_PROMO) && MOVE_PROMO(m) != promo_w) continue;
        if (df >= 0 && (from % 8) != df) continue;
        if (dr >= 0 && (from / 8) != dr) continue;

        *out = m;
        return true;
    }
    return false;
}

bool san_apply(Board *b, const char *san)
{
    Move m;
    if (!san_find(b, san, &m)) return false;
    make_move_plumb(b, m);
    return true;
}

static int classify(const Board *b, const char *san)
{
    Board tmp = *b;
    if (!san_apply(&tmp, san)) return -1;
    return (int)game_state(&tmp);
}

int san_classify(const Board *b, const char *san)
{
    return classify(b, san);
}

/* --- SAN generation --- */
void move_to_san(const Board *before, Move m, char *out, size_t n)
{
    int from = MOVE_FROM(m), to = MOVE_TO(m);
    uint32_t fl = MOVE_FLAGS(m);
    Piece p = before->board[from];

    if (out == NULL || n == 0) return;
    char buf[16] = {0};
    size_t k = 0;

    if (fl & FLAG_CASTLE_K) {
        snprintf(buf, sizeof buf, "O-O");
    } else if (fl & FLAG_CASTLE_Q) {
        snprintf(buf, sizeof buf, "O-O-O");
    } else {
        bool is_pawn = (p == WP || p == BP);
        bool is_capture = (fl & FLAG_CAPTURE) || (fl & FLAG_EP) || (before->board[to] != EMPTY);

        if (!is_pawn) {
            char pl = 'N';
            switch (p) { case WR: case BR: pl='R'; break; case WN: case BN: pl='N'; break;
                         case WB: case BB: pl='B'; break; case WQ: case BQ: pl='Q'; break;
                         case WK: case BK: pl='K'; break; default: pl='N'; break; }
            buf[k++] = pl;
        }

        /* disambiguation */
        MoveList legal;
        gen_legal(before, &legal);
        bool conflict_file = false, conflict_rank = false, any = false;
        for (int i = 0; i < legal.count; i++) {
            Move o = legal.moves[i];
            if (o == m) continue;
            if (before->board[MOVE_FROM(o)] != p) continue;
            if (MOVE_TO(o) != to) continue;
            any = true;
            if ((MOVE_FROM(o) % 8) == (from % 8)) conflict_file = true;
            if ((MOVE_FROM(o) / 8) == (from / 8)) conflict_rank = true;
        }
        if (any && conflict_file && conflict_rank) {
            buf[k++] = (char)('a' + (from % 8));
            buf[k++] = (char)('1' + (from / 8));
        } else if (any && conflict_file) {
            buf[k++] = (char)('1' + (from / 8));
        } else if (any && conflict_rank) {
            buf[k++] = (char)('a' + (from % 8));
        }

        if (is_capture) {
            if (is_pawn) buf[k++] = (char)('a' + (from % 8));
            buf[k++] = 'x';
        }

        char alg[3];
        sq_to_algebraic(to, alg);
        buf[k++] = alg[0];
        buf[k++] = alg[1];

        if ((fl & FLAG_PROMO) || ((p == WP || p == BP) && (to / 8 == 7 || to / 8 == 0))) {
            Piece pr = (fl & FLAG_PROMO) ? MOVE_PROMO(m)
                       : ((p == WP) ? WQ : BQ);
            char pl2 = 'Q';
            switch (pr) { case WN: case BN: pl2='N'; break; case WR: case BR: pl2='R'; break;
                          case WB: case BB: pl2='B'; break; case WQ: case BQ: pl2='Q'; break;
                          default: break; }
            buf[k++] = '=';
            buf[k++] = pl2;
        }
    }

    /* check / checkmate suffix */
    /* Determine if the move gives check/mate by applying to a copy */
    Board tmp = *before;
    make_move_plumb(&tmp, m);
    MoveList opp_legal;
    gen_legal(&tmp, &opp_legal);
    if (in_check(&tmp, tmp.side)) {
        if (opp_legal.count == 0) buf[k++] = '#';
        else buf[k++] = '+';
    }

    buf[k] = '\0';
    strncpy(out, buf, n - 1);
    out[n - 1] = '\0';
}