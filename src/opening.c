#include "opening.h"
#include "move.h"
#include "pgn.h"
#include "fen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BOOK_DEPTH 24
#define POS_LEN 96

typedef struct {
    char eco[8];
    char name[96];
    char moves[256];   /* SAN movetext (move numbers included) */
    char keys[256];    /* SAN only, space separated, for path matching */
    int  key_len;
} OpeningLine;

/* Replay `pgn` and write its SAN moves (space separated) into `out`. */
static int build_keys(const char *pgn, char *out, size_t n)
{
    Board board;
    board_reset(&board);
    out[0] = '\0';
    size_t used = 0;
    int count = 0;
    const char *p = pgn;
    while (*p && count < MAX_BOOK_DEPTH) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char tok[16];
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < 15) tok[i++] = *p++;
        tok[i] = '\0';
        if (strcmp(tok, "1-0") == 0 || strcmp(tok, "0-1") == 0 ||
            strcmp(tok, "1/2-1/2") == 0 || strcmp(tok, "*") == 0) break;
        if (tok[0] >= '0' && tok[0] <= '9' &&
            strspn(tok, "0123456789.") == strlen(tok)) continue;
        Move m;
        if (!san_find(&board, tok, &m)) break;
        make_move_plumb(&board, m);
        if (used + strlen(tok) + 2 >= n) break;
        if (used) { out[used++] = ' '; }
        memcpy(out + used, tok, strlen(tok));
        used += strlen(tok);
        out[used] = '\0';
        count++;
    }
    return count;
}

struct OpeningBook {
    char (*pos)[POS_LEN];
    char (*pos_name)[96];   /* opening name for each position */
    char (*pos_eco)[8];     /* ECO code for each position */
    int  *pos_depth;        /* plies into the line (most specific wins) */
    int count, cap;
    OpeningLine *lines;
    int line_count, line_cap;
};

static void normalize(const Board *b, char out[POS_LEN])
{
    char fen[128];
    fen_generate(b, fen, sizeof fen);
    /* keep the first four FEN fields (drop halfmove/fullmove) */
    int spaces = 0;
    size_t i = 0;
    for (; fen[i] && spaces < 4 && i < POS_LEN - 1; i++) {
        if (fen[i] == ' ') spaces++;
        out[i] = fen[i];
    }
    out[i] = '\0';
}

static bool book_add(OpeningBook *b, const Board *pos, const char *name,
                     const char *eco, int depth)
{
    char norm[POS_LEN];
    normalize(pos, norm);
    for (int i = 0; i < b->count; i++)
        if (strcmp(b->pos[i], norm) == 0) {
            /* keep the most specific (deepest) name for this position */
            if (depth > b->pos_depth[i]) {
                b->pos_depth[i] = depth;
                snprintf(b->pos_name[i], 96, "%s", name ? name : "");
                snprintf(b->pos_eco[i], 8, "%s", eco ? eco : "");
            }
            return false;
        }

    if (b->count == b->cap) {
        int nc = b->cap ? b->cap * 2 : 4096;
        char (*np)[POS_LEN] = realloc(b->pos, (size_t)nc * POS_LEN);
        if (!np) return false;
        b->pos = np;
        char (*nn)[96] = realloc(b->pos_name, (size_t)nc * 96);
        if (!nn) return false;
        b->pos_name = nn;
        char (*ne)[8] = realloc(b->pos_eco, (size_t)nc * 8);
        if (!ne) return false;
        b->pos_eco = ne;
        int *nd = realloc(b->pos_depth, (size_t)nc * sizeof *nd);
        if (!nd) return false;
        b->pos_depth = nd;
        b->cap = nc;
    }
    snprintf(b->pos[b->count], POS_LEN, "%s", norm);
    snprintf(b->pos_name[b->count], 96, "%s", name ? name : "");
    snprintf(b->pos_eco[b->count], 8, "%s", eco ? eco : "");
    b->pos_depth[b->count] = depth;
    b->count++;
    return true;
}

static void add_line(OpeningBook *b, const char *pgn, const char *name,
                     const char *eco)
{
    Board board;
    board_reset(&board);
    book_add(b, &board, name, eco, 0);

    const char *p = pgn;
    int ply = 0;
    while (*p && ply < MAX_BOOK_DEPTH) {

        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char tok[16];
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && i < 15) tok[i++] = *p++;
        tok[i] = '\0';

        /* stop at result tokens */
        if (strcmp(tok, "1-0") == 0 || strcmp(tok, "0-1") == 0 ||
            strcmp(tok, "1/2-1/2") == 0 || strcmp(tok, "*") == 0)
            break;
        /* skip move numbers like "1." or "12..." */
        if (tok[0] >= '0' && tok[0] <= '9' &&
            strspn(tok, "0123456789.") == strlen(tok))
            continue;

        Move m;
        if (!san_find(&board, tok, &m)) break;
        make_move_plumb(&board, m);
        ply++;
        book_add(b, &board, name, eco, ply);
    }
}

OpeningBook *opening_load(const char *path)
{
    if (!path || !*path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    OpeningBook *b = calloc(1, sizeof *b);
    if (!b) { fclose(f); return NULL; }

    char line[1024];
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n == 0 || line[0] == '#') continue;

        /* Columns: eco<TAB>name<TAB>pgn. Skip the header row. */
        char *t1 = strchr(line, '\t');
        char *pgn = t1 ? strrchr(t1 + 1, '\t') : strrchr(line, '\t');
        pgn = pgn ? pgn + 1 : line;
        if (strcmp(line, "eco") == 0 || strncmp(line, "eco\t", 4) == 0) continue;

        char namebuf[96] = "";
        char ecobuf[8] = "";
        if (t1) {
            size_t el = (size_t)(t1 - line);
            if (el >= sizeof ecobuf) el = sizeof ecobuf - 1;
            memcpy(ecobuf, line, el);
            ecobuf[el] = '\0';
            char *nm = t1 + 1;
            char *t2 = strchr(nm, '\t');
            size_t nl = t2 ? (size_t)(t2 - nm) : strlen(nm);
            if (nl >= sizeof namebuf) nl = sizeof namebuf - 1;
            memcpy(namebuf, nm, nl);
            namebuf[nl] = '\0';
        }

        add_line(b, pgn, namebuf, ecobuf);

        /* store the line for the browse mode */
        if (b->line_count == b->line_cap) {
            int nc = b->line_cap ? b->line_cap * 2 : 512;
            OpeningLine *nl = realloc(b->lines, (size_t)nc * sizeof *nl);
            if (!nl) break;
            b->lines = nl;
            b->line_cap = nc;
        }
        OpeningLine *L = &b->lines[b->line_count++];
        memset(L, 0, sizeof *L);
        if (t1) {
            size_t el = (size_t)(t1 - line);
            if (el >= sizeof L->eco) el = sizeof L->eco - 1;
            memcpy(L->eco, line, el);
            L->eco[el] = '\0';
            char *name = t1 + 1;
            char *t2 = strchr(name, '\t');
            size_t nl = t2 ? (size_t)(t2 - name) : strlen(name);
            if (nl >= sizeof L->name) nl = sizeof L->name - 1;
            memcpy(L->name, name, nl);
            L->name[nl] = '\0';
        }
        snprintf(L->moves, sizeof L->moves, "%s", pgn);
        L->key_len = build_keys(pgn, L->keys, sizeof L->keys);
    }
    fclose(f);

    if (b->count == 0) { opening_free(b); return NULL; }
    return b;
}

void opening_free(OpeningBook *b)
{
    if (!b) return;
    free(b->pos);
    free(b->pos_name);
    free(b->pos_eco);
    free(b->pos_depth);
    free(b->lines);
    free(b);
}

int opening_count(const OpeningBook *b) { return b ? b->count : 0; }

int opening_line_count(const OpeningBook *b) { return b ? b->line_count : 0; }

bool opening_line_at(const OpeningBook *b, int index,
                     char *eco, size_t eco_n, char *name, size_t name_n,
                     char *moves, size_t moves_n)
{
    if (!b || index < 0 || index >= b->line_count) return false;
    OpeningLine *L = &b->lines[index];
    snprintf(eco, eco_n, "%s", L->eco);
    snprintf(name, name_n, "%s", L->name);
    snprintf(moves, moves_n, "%s", L->moves);
    return true;
}

bool opening_is_book(const OpeningBook *b, const Board *pos)
{
    if (!b || !pos) return false;
    char norm[POS_LEN];
    normalize(pos, norm);
    for (int i = 0; i < b->count; i++)
        if (strcmp(b->pos[i], norm) == 0) return true;
    return false;
}

bool opening_name_at(const OpeningBook *b, const Board *pos, char *out, size_t n)
{
    if (out && n) out[0] = '\0';
    if (!b || !pos) return false;
    char norm[POS_LEN];
    normalize(pos, norm);
    for (int i = 0; i < b->count; i++)
        if (strcmp(b->pos[i], norm) == 0) {
            if (out && n) snprintf(out, n, "%s", b->pos_name[i]);
            return true;
        }
    return false;
}

bool opening_pos_info(const OpeningBook *b, const Board *pos,
                      char *eco, size_t eco_n, char *name, size_t name_n)
{
    if (eco && eco_n) eco[0] = '\0';
    if (name && name_n) name[0] = '\0';
    if (!b || !pos) return false;
    char norm[POS_LEN];
    normalize(pos, norm);
    for (int i = 0; i < b->count; i++)
        if (strcmp(b->pos[i], norm) == 0) {
            if (eco && eco_n) snprintf(eco, eco_n, "%s", b->pos_eco[i]);
            if (name && name_n) snprintf(name, name_n, "%s", b->pos_name[i]);
            return true;
        }
    return false;
}

/* Replay a line's SAN keys and report whether `board` occurs along it. */
static bool line_has_position(const Board *start, const char *keys,
                              int key_len, const char *want_norm)
{
    Board b = *start;
    char norm[POS_LEN];
    normalize(&b, norm);
    if (strcmp(norm, want_norm) == 0) return true;

    const char *k = keys;
    for (int j = 0; j < key_len; j++) {
        while (*k == ' ') k++;
        if (!*k) break;
        char tok[16];
        int n = 0;
        while (*k && *k != ' ' && n < 15) tok[n++] = *k++;
        tok[n] = '\0';
        Move m;
        if (!san_find(&b, tok, &m)) break;
        make_move_plumb(&b, m);
        normalize(&b, norm);
        if (strcmp(norm, want_norm) == 0) return true;
    }
    return false;
}

int opening_line_next(const OpeningBook *b, int index, const Board *pos,
                      char *out, size_t n)
{
    if (out && n) out[0] = '\0';
    if (!b || index < 0 || index >= b->line_count || !pos) return -1;
    OpeningLine *L = &b->lines[index];

    Board start;
    board_reset(&start);
    Board bd = start;
    char norm[POS_LEN];
    normalize(&bd, norm);
    char want[POS_LEN];
    normalize(pos, want);

    const char *k = L->keys;
    for (int j = 0; j <= L->key_len; j++) {
        if (strcmp(norm, want) == 0) {
            if (j >= L->key_len) return 0;   /* position is the end of the line */
            char tok[16];
            int m = 0;
            while (*k == ' ') k++;
            while (*k && *k != ' ' && m < 15) tok[m++] = *k++;
            tok[m] = '\0';
            if (out && n) snprintf(out, n, "%s", tok);
            return 1;
        }
        if (j == L->key_len) break;
        while (*k == ' ') k++;
        if (!*k) break;
        char tok[16];
        int m = 0;
        while (*k && *k != ' ' && m < 15) tok[m++] = *k++;
        tok[m] = '\0';
        Move mv;
        if (!san_find(&bd, tok, &mv)) break;
        make_move_plumb(&bd, mv);
        normalize(&bd, norm);
    }
    return -1;
}

int opening_lines_at(const OpeningBook *b, const Board *pos, int *out, int max)
{
    if (!b || !pos || !out || max <= 0) return 0;
    char norm[POS_LEN];
    normalize(pos, norm);

    Board start;
    board_reset(&start);
    int n = 0;
    for (int li = 0; li < b->line_count && n < max; li++) {
        OpeningLine *L = &b->lines[li];
        if (L->key_len == 0) continue;
        if (line_has_position(&start, L->keys, L->key_len, norm))
            out[n++] = li;
    }
    return n;
}

int opening_lookup(const OpeningBook *b, const char (*sans)[8], int n,
                   char *eco, size_t eco_n, char *name, size_t name_n)
{
    if (eco && eco_n) eco[0] = '\0';
    if (name && name_n) name[0] = '\0';
    if (!b || !sans || n <= 0) return 0;

    int best = 0;              /* matched plies */
    int best_len = 0;          /* line length (tie-break: more specific) */
    int best_line = -1;

    for (int li = 0; li < b->line_count; li++) {
        OpeningLine *L = &b->lines[li];
        if (L->key_len == 0) continue;
        int lim = n < L->key_len ? n : L->key_len;
        const char *k = L->keys;
        int matched = 0, ok = 1;
        for (int j = 0; j < lim; j++) {
            while (*k == ' ') k++;
            if (!*k) { ok = 0; break; }
            const char *s = k;
            while (*k && *k != ' ') k++;
            size_t kl = (size_t)(k - s);
            size_t sl = strlen(sans[j]);
            if (kl != sl || strncmp(s, sans[j], kl) != 0) { ok = 0; break; }
            matched++;
        }
        if (!ok || matched == 0) continue;
        if (matched > best || (matched == best && L->key_len > best_len)) {
            best = matched;
            best_len = L->key_len;
            best_line = li;
        }
        if (best == n && best_len == n) break;   /* exact full match */
    }

    if (best_line >= 0) {
        OpeningLine *L = &b->lines[best_line];
        if (eco && eco_n) snprintf(eco, eco_n, "%s", L->eco);
        if (name && name_n) snprintf(name, name_n, "%s", L->name);
        return best;
    }
    return 0;
}
