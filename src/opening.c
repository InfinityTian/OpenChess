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
} OpeningLine;

struct OpeningBook {
    char (*pos)[POS_LEN];
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

static bool book_add(OpeningBook *b, const Board *pos)
{
    char norm[POS_LEN];
    normalize(pos, norm);
    for (int i = 0; i < b->count; i++)
        if (strcmp(b->pos[i], norm) == 0) return false;   /* already present */

    if (b->count == b->cap) {
        int nc = b->cap ? b->cap * 2 : 4096;
        char (*np)[POS_LEN] = realloc(b->pos, (size_t)nc * POS_LEN);
        if (!np) return false;
        b->pos = np;
        b->cap = nc;
    }
    snprintf(b->pos[b->count], POS_LEN, "%s", norm);
    b->count++;
    return true;
}

static void add_line(OpeningBook *b, const char *pgn)
{
    Board board;
    board_reset(&board);
    book_add(b, &board);

    const char *p = pgn;
    int ply = 0;
    (void)ply;
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
        book_add(b, &board);
        ply++;
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

        add_line(b, pgn);

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
    }
    fclose(f);

    if (b->count == 0) { opening_free(b); return NULL; }
    return b;
}

void opening_free(OpeningBook *b)
{
    if (!b) return;
    free(b->pos);
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
