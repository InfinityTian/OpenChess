#include "puzzle.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Puzzles {
    Puzzle *items;
    int     count;
    int     cap;
};

static void copy_str(char *dst, size_t n, const char *src)
{
    snprintf(dst, n, "%s", src ? src : "");
}

static bool parse_puzzle(const char *line, Puzzle *out)
{
    cJSON *root = cJSON_Parse(line);
    if (!root) return false;

    memset(out, 0, sizeof *out);
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    const cJSON *fen = cJSON_GetObjectItemCaseSensitive(root, "fen");
    const cJSON *moves = cJSON_GetObjectItemCaseSensitive(root, "moves");
    const cJSON *rating = cJSON_GetObjectItemCaseSensitive(root, "rating");
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(root, "themes");

    if (!cJSON_IsString(fen) || !cJSON_IsString(moves)) {
        cJSON_Delete(root);
        return false;
    }
    copy_str(out->id, sizeof out->id, cJSON_IsString(id) ? id->valuestring : "");
    copy_str(out->fen, sizeof out->fen, fen->valuestring);
    copy_str(out->moves, sizeof out->moves, moves->valuestring);
    out->rating = cJSON_IsNumber(rating) ? (int)rating->valuedouble : 1500;

    out->themes[0] = '\0';
    if (cJSON_IsArray(themes)) {
        size_t used = 0;
        const cJSON *t = NULL;
        cJSON_ArrayForEach(t, themes) {
            if (!cJSON_IsString(t) || !t->valuestring) continue;
            size_t l = strlen(t->valuestring);
            if (used && used + 1 < sizeof out->themes) out->themes[used++] = '|';
            if (used + l >= sizeof out->themes) break;
            memcpy(out->themes + used, t->valuestring, l);
            used += l;
            out->themes[used] = '\0';
        }
    }
    cJSON_Delete(root);
    return out->fen[0] && out->moves[0];
}

Puzzles *puzzles_load(const char *path)
{
    if (!path || !*path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    Puzzles *p = calloc(1, sizeof *p);
    if (!p) { fclose(f); return NULL; }

    char line[4096];
    while (fgets(line, sizeof line, f)) {
        size_t len = strlen(line);
        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0 || line[0] != '{') continue;

        Puzzle tmp;
        if (!parse_puzzle(line, &tmp)) continue;
        if (p->count == p->cap) {
            int nc = p->cap ? p->cap * 2 : 256;
            Puzzle *ni = realloc(p->items, (size_t)nc * sizeof *ni);
            if (!ni) break;
            p->items = ni;
            p->cap = nc;
        }
        p->items[p->count++] = tmp;
    }
    fclose(f);

    if (p->count == 0) { puzzles_free(p); return NULL; }
    return p;
}

void puzzles_free(Puzzles *p)
{
    if (!p) return;
    free(p->items);
    free(p);
}

int puzzles_count(const Puzzles *p) { return p ? p->count : 0; }

bool puzzles_get(const Puzzles *p, int index, Puzzle *out)
{
    if (!p || index < 0 || index >= p->count) return false;
    *out = p->items[index];
    return true;
}

static bool matches(const Puzzle *z, int rmin, int rmax, const char *theme)
{
    if (z->rating < rmin || z->rating > rmax) return false;
    if (theme && *theme && !strstr(z->themes, theme)) return false;
    return true;
}

bool puzzles_pick(const Puzzles *p, int rmin, int rmax, const char *theme,
                  Puzzle *out)
{
    if (!p || p->count == 0) return false;

    int total = 0;
    for (int i = 0; i < p->count; i++)
        if (matches(&p->items[i], rmin, rmax, theme)) total++;
    if (total == 0) return false;

    int pick = rand() % total;
    for (int i = 0; i < p->count; i++) {
        if (!matches(&p->items[i], rmin, rmax, theme)) continue;
        if (pick-- == 0) { if (out) *out = p->items[i]; return true; }
    }
    return false;
}
