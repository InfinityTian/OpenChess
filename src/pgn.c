#include "pgn.h"
#include "fen.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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
        k = strlen(buf);
    } else if (fl & FLAG_CASTLE_Q) {
        snprintf(buf, sizeof buf, "O-O-O");
        k = strlen(buf);
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

        /* Disambiguation (pieces only): pawns are disambiguated by the origin
         * file that the capture prefix already includes. */
        if (!is_pawn) {
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
                /* same file as another candidate -> rank disambiguates */
                buf[k++] = (char)('1' + (from / 8));
            } else if (any && conflict_rank) {
                /* same rank as another candidate -> file disambiguates */
                buf[k++] = (char)('a' + (from % 8));
            } else if (any) {
                /* different file and rank -> the departure file is unique */
                buf[k++] = (char)('a' + (from % 8));
            }
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

/* --- PGN export --- */

const char *pgn_result(const Board *b, GameState state)
{
    switch (state) {
        case CHECKMATE:
            return (b->side == WHITE) ? "0-1" : "1-0";
        case STALEMATE:
        case INSUFFICIENT_MATERIAL:
        case THREEFOLD_REPETITION:
        case FIFTY_MOVE_RULE:
            return "1/2-1/2";
        default:
            return "*";
    }
}

static void write_header(FILE *f, const char *tag, const char *value)
{
    fprintf(f, "[%s \"", tag);
    for (const char *p = value; p && *p; p++) {
        if (*p == '"' || *p == '\\') fputc('\\', f);
        fputc(*p, f);
    }
    fprintf(f, "\"]\n");
}

void pgn_write(FILE *f, const char moves[][8], int ply,
               const char *event, const char *site, const char *date,
               int round, const char *white, const char *black,
               const char *result)
{
    if (!f) return;

    write_header(f, "Event",  event  && *event  ? event  : "?");
    write_header(f, "Site",   site   && *site   ? site   : "?");
    write_header(f, "Date",   date   && *date   ? date   : "????.??.??");
    char roundbuf[16];
    snprintf(roundbuf, sizeof roundbuf, "%d", round > 0 ? round : 1);
    write_header(f, "Round",  roundbuf);
    write_header(f, "White",  white  && *white  ? white  : "?");
    write_header(f, "Black",  black  && *black  ? black  : "?");
    write_header(f, "Result", result && *result ? result : "*");
    fputc('\n', f);

    int col = 0;
    for (int i = 0; i < ply; i++) {
        char tok[32];
        if (i % 2 == 0) snprintf(tok, sizeof tok, "%d. %s", i / 2 + 1, moves[i]);
        else            snprintf(tok, sizeof tok, "%s", moves[i]);

        int len = (int)strlen(tok);
        if (col > 0 && col + 1 + len > 80) { fputc('\n', f); col = 0; }
        else if (col > 0) { fputc(' ', f); col++; }
        fputs(tok, f);
        col += len;
    }

    const char *res = (result && *result) ? result : "*";
    int len = (int)strlen(res);
    if (col > 0 && col + 1 + len > 80) { fputc('\n', f); col = 0; }
    else if (col > 0) { fputc(' ', f); col++; }
    fputs(res, f);
    fputc('\n', f);
}
/* ---- PGN import (variations/NAGs/comments) ------------------------------ */

static const char *parse_moves(const char *p, MoveNode **cur,
                               char *res, size_t rn);

static void parse_header_line(const char *start, const char *end, PgnHeaders *out,
                              Board *fen_out, bool *have_fen)
{
    char tag[32] = "", val[160] = "";
    const char *q = start + 1;
    while (q < end && (*q == ' ' || *q == '\t')) q++;
    int i = 0;
    while (q < end && *q != ' ' && *q != '\t' && *q != ']' && i < 31) tag[i++] = *q++;
    tag[i] = '\0';
    while (q < end && (*q == ' ' || *q == '\t')) q++;
    if (q < end && *q == '"') q++;
    i = 0;
    while (q < end && *q != '"' && i < 159) val[i++] = *q++;
    val[i] = '\0';

    if (!out) return;
    if (strcasecmp(tag, "Event") == 0) snprintf(out->event, sizeof out->event, "%s", val);
    else if (strcasecmp(tag, "Site") == 0) snprintf(out->site, sizeof out->site, "%s", val);
    else if (strcasecmp(tag, "Date") == 0) snprintf(out->date, sizeof out->date, "%s", val);
    else if (strcasecmp(tag, "Round") == 0) snprintf(out->round, sizeof out->round, "%s", val);
    else if (strcasecmp(tag, "White") == 0) snprintf(out->white, sizeof out->white, "%s", val);
    else if (strcasecmp(tag, "Black") == 0) snprintf(out->black, sizeof out->black, "%s", val);
    else if (strcasecmp(tag, "Result") == 0) snprintf(out->result, sizeof out->result, "%s", val);
    else if (strcasecmp(tag, "FEN") == 0 && fen_out) {
        Board b;
        if (fen_parse(val, &b)) { *fen_out = b; *have_fen = true; }
    }
}

static const char *parse_moves(const char *p, MoveNode **cur,
                               char *res, size_t rn)
{
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (!*p) return p;
        if (*p == ')') return p;

        if (*p == '{') {
            const char *e = strchr(p, '}');
            if (!e) return p + strlen(p);
            if (*cur) {
                size_t n = (size_t)(e - p - 1);
                if (n >= sizeof (*cur)->comment) n = sizeof (*cur)->comment - 1;
                memcpy((*cur)->comment, p + 1, n);
                (*cur)->comment[n] = '\0';
            }
            p = e + 1;
            continue;
        }
        if (*p == ';') { while (*p && *p != '\n') p++; continue; }
        if (*p == '$') {
            int v = 0; p++;
            while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
            if (*cur) (*cur)->nag = v;
            continue;
        }
        if (*p == '(') {
            MoveNode *save = *cur;
            if (save && save->parent) {
                MoveNode *vc = save->parent;
                p = parse_moves(p + 1, &vc, res, rn);
            } else {
                int depth = 1; p++;
                while (*p && depth) { if (*p == '(') depth++; else if (*p == ')') depth--; p++; }
                continue;
            }
            if (*p == ')') p++;
            continue;
        }
        if (*p >= '0' && *p <= '9') {
            if (strncmp(p, "1-0", 3) == 0) { if (res) snprintf(res, rn, "1-0"); return p + 3; }
            if (strncmp(p, "0-1", 3) == 0) { if (res) snprintf(res, rn, "0-1"); return p + 3; }
            if (strncmp(p, "1/2-1/2", 7) == 0) { if (res) snprintf(res, rn, "1/2-1/2"); return p + 7; }
            while (*p >= '0' && *p <= '9') p++;
            while (*p == '.') p++;
            continue;
        }
        if (*p == '*') { if (res) snprintf(res, rn, "*"); return p + 1; }

        char tok[16];
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' &&
               *p != '(' && *p != ')' && *p != '{' && *p != '$' && i < 15)
            tok[i++] = *p++;
        tok[i] = '\0';
        while (i > 0 && tok[i - 1] == '.') tok[--i] = '\0';
        if (i == 0) continue;

        Move m;
        if (*cur && san_find(&(*cur)->board, tok, &m))
            *cur = mt_add_child(*cur, m, NULL);
    }
}

MoveNode *pgn_parse_text(const char *text, PgnHeaders *out)
{
    Board start;
    board_reset(&start);
    bool have_fen = false;
    if (out) memset(out, 0, sizeof *out);

    const char *p = text ? text : "";
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != '[') break;
        const char *e = strchr(p, ']');
        if (!e) break;
        parse_header_line(p, e, out, &start, &have_fen);
        p = e + 1;
    }
    (void)have_fen;

    MoveNode *root = mt_new_root(&start);
    MoveNode *cur = root;
    parse_moves(p, &cur, out ? out->result : NULL, out ? sizeof out->result : 0);
    return root;
}

typedef struct { char *p; size_t len, cap; } Buf;

static void buf_reserve(Buf *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 1024;
    while (nc < b->len + extra + 1) nc *= 2;
    b->p = realloc(b->p, nc);
    b->cap = nc;
}

static void buf_str(Buf *b, const char *s)
{
    size_t l = strlen(s);
    buf_reserve(b, l);
    memcpy(b->p + b->len, s, l);
    b->len += l;
    b->p[b->len] = '\0';
}

static void buf_ch(Buf *b, char c)
{
    buf_reserve(b, 1);
    b->p[b->len++] = c;
    b->p[b->len] = '\0';
}

static void emit_token(const MoveNode *n, Buf *b)
{
    int d = mt_depth(n);
    bool white = (d % 2 == 1);
    if (white) {
        char num[16];
        snprintf(num, sizeof num, "%d. ", (d + 1) / 2);
        buf_str(b, num);
    }
    buf_str(b, n->san[0] ? n->san : "?");
    if (n->nag) {
        /* Always use the standard numeric NAG form ($1, $2, ...) - never the
         * "!?" glyph suffix, which is not valid PGN movetext. */
        char t[16];
        snprintf(t, sizeof t, " $%d", n->nag);
        buf_str(b, t);
    }
    if (n->comment[0]) { buf_str(b, " {"); buf_str(b, n->comment); buf_ch(b, '}'); }
    buf_ch(b, ' ');
}

/* Emit a move, then its sibling alternatives as variations (placed right after
 * it), then continue to its child. */
static void emit_line(const MoveNode *first, Buf *b);

static void emit_variation(const MoveNode *v, Buf *b)
{
    emit_token(v, b);
    if (v->first) emit_line(v->first, b);
}

static void emit_line(const MoveNode *first, Buf *b)
{
    for (const MoveNode *m = first; m; m = m->first) {
        emit_token(m, b);
        for (const MoveNode *alt = m->next; alt; alt = alt->next) {
            buf_str(b, "(");
            emit_variation(alt, b);
            buf_str(b, ") ");
        }
    }
}

char *pgn_serialize(const MoveNode *root, const PgnHeaders *h)
{
    Buf b = {0};
    buf_reserve(&b, 1);
    b.p[0] = '\0';

    if (h) {
        if (h->event[0]) { buf_str(&b, "[Event \""); buf_str(&b, h->event); buf_str(&b, "\"]\n"); }
        if (h->site[0])  { buf_str(&b, "[Site \"");  buf_str(&b, h->site);  buf_str(&b, "\"]\n"); }
        if (h->date[0])  { buf_str(&b, "[Date \"");  buf_str(&b, h->date);  buf_str(&b, "\"]\n"); }
        if (h->round[0]) { buf_str(&b, "[Round \""); buf_str(&b, h->round); buf_str(&b, "\"]\n"); }
        if (h->white[0]) { buf_str(&b, "[White \""); buf_str(&b, h->white); buf_str(&b, "\"]\n"); }
        if (h->black[0]) { buf_str(&b, "[Black \""); buf_str(&b, h->black); buf_str(&b, "\"]\n"); }
        if (h->result[0]){ buf_str(&b, "[Result \"");buf_str(&b, h->result);buf_str(&b, "\"]\n"); }
        buf_ch(&b, '\n');
    }

    if (root && root->first) {
        emit_line(root->first, &b);
        buf_ch(&b, '\n');
        if (h && h->result[0]) { buf_str(&b, h->result); buf_ch(&b, '\n'); }
    } else {
        buf_str(&b, "*\n");
    }
    return b.p;
}
