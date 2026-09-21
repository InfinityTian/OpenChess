#include "themes.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

static bool file_exists(const char *path)
{
    return access(path, R_OK) == 0;
}

static bool is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void join_path(char *out, size_t n, const char *dir, const char *rel)
{
    size_t len = strlen(dir);
    if (len > 0 && dir[len - 1] == '/')
        snprintf(out, n, "%s%s", dir, rel);
    else
        snprintf(out, n, "%s/%s", dir, rel);
}

static void list_push(ThemeList *list, const ThemeEntry *e)
{
    ThemeEntry *grown = realloc(list->items, (size_t)(list->count + 1) * sizeof *grown);
    if (!grown) return;
    list->items = grown;
    list->items[list->count++] = *e;
}

static void add_builtin_board(ThemeList *l, const char *key, const char *label,
                              unsigned char lr, unsigned char lg, unsigned char lb,
                              unsigned char dr, unsigned char dg, unsigned char db,
                              unsigned char cr, unsigned char cg, unsigned char cb)
{
    ThemeEntry e;
    memset(&e, 0, sizeof e);
    snprintf(e.key, sizeof e.key, "%s", key);
    snprintf(e.label, sizeof e.label, "%s", label);
    e.builtin = true;
    e.light[0] = lr; e.light[1] = lg; e.light[2] = lb;
    e.dark[0]  = dr; e.dark[1]  = dg; e.dark[2]  = db;
    e.coord[0] = cr; e.coord[1] = cg; e.coord[2] = cb;
    list_push(l, &e);
}

static int cmp_entry(const void *a, const void *b)
{
    const ThemeEntry *x = a, *y = b;
    return strcmp(x->key, y->key);
}

static void add_builtins(ThemeList *boards, ThemeList *pieces)
{
    /* Plain procedural boards and the glyph font set always exist. */
    add_builtin_board(boards, "classic_plain", "Classic (plain)",
                      240, 217, 181, 181, 136, 99, 40, 30, 20);
    add_builtin_board(boards, "slate_plain", "Slate (plain)",
                      188, 198, 210, 78, 90, 106, 20, 25, 35);

    ThemeEntry e;
    memset(&e, 0, sizeof e);
    snprintf(e.key, sizeof e.key, "glyph");
    snprintf(e.label, sizeof e.label, "Glyph");
    e.builtin = true;
    list_push(pieces, &e);
}

/* --- manifest -------------------------------------------------------- */

static void load_manifest(ThemeList *boards, ThemeList *pieces,
                          const char *assets_dir)
{
    char path[THEME_PATH_MAX];
    join_path(path, sizeof path, assets_dir, "themes.txt");
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';
        char *s = trim(line);
        if (!*s) continue;

        char *kind = strtok(s, " \t");
        char *key = strtok(NULL, " \t");
        if (!kind || !key) continue;
        char *label = strtok(NULL, "\n");   /* rest of the line, may contain spaces */
        if (label) label = trim(label);

        ThemeEntry e;
        memset(&e, 0, sizeof e);
        snprintf(e.key, sizeof e.key, "%s", key);
        snprintf(e.label, sizeof e.label, "%s", (label && *label) ? label : key);

        if (strcmp(kind, "board") == 0) {
            char p[THEME_PATH_MAX];
            char rel[256];
            snprintf(rel, sizeof rel, "boards/%s.png", e.key);
            join_path(p, sizeof p, assets_dir, rel);
            if (!file_exists(p)) continue;
            snprintf(e.path, sizeof e.path, "%s", p);
            list_push(boards, &e);
        } else if (strcmp(kind, "piece") == 0) {
            char d[THEME_PATH_MAX], rel[256], probe[THEME_PATH_MAX];
            snprintf(rel, sizeof rel, "pieces/%s", e.key);
            join_path(d, sizeof d, assets_dir, rel);
            join_path(probe, sizeof probe, d, "wp.png");
            if (!file_exists(probe)) continue;
            snprintf(e.path, sizeof e.path, "%s", d);
            list_push(pieces, &e);
        }
    }
    fclose(f);
}

/* --- directory scans (fallback) -------------------------------------- */

static void scan_boards(ThemeList *boards, const char *assets_dir)
{
    char dir[THEME_PATH_MAX];
    join_path(dir, sizeof dir, assets_dir, "boards");
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d))) {
        const char *name = de->d_name;
        size_t len = strlen(name);
        if (len < 5 || strcmp(name + len - 4, ".png") != 0) continue;

        ThemeEntry e;
        memset(&e, 0, sizeof e);
        snprintf(e.key, sizeof e.key, "%.*s", (int)(len - 4), name);
        snprintf(e.label, sizeof e.label, "%s", e.key);
        join_path(e.path, sizeof e.path, dir, name);
        list_push(boards, &e);
    }
    closedir(d);
}

static void scan_pieces(ThemeList *pieces, const char *assets_dir)
{
    char dir[THEME_PATH_MAX];
    join_path(dir, sizeof dir, assets_dir, "pieces");
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.') continue;
        char sub[THEME_PATH_MAX], probe[THEME_PATH_MAX];
        join_path(sub, sizeof sub, dir, de->d_name);
        if (!is_dir(sub)) continue;
        join_path(probe, sizeof probe, sub, "wp.png");
        if (!file_exists(probe)) continue;

        ThemeEntry e;
        memset(&e, 0, sizeof e);
        snprintf(e.key, sizeof e.key, "%s", de->d_name);
        snprintf(e.label, sizeof e.label, "%s", e.key);
        snprintf(e.path, sizeof e.path, "%s", sub);
        list_push(pieces, &e);
    }
    closedir(d);
}

/* --- public ---------------------------------------------------------- */

bool themes_load(ThemeList *boards, ThemeList *pieces, const char *assets_dir)
{
    if (!boards || !pieces) return false;
    boards->items = NULL; boards->count = 0;
    pieces->items = NULL; pieces->count = 0;

    const char *dir = (assets_dir && *assets_dir) ? assets_dir : "assets";

    load_manifest(boards, pieces, dir);

    /* Preserve the manifest's curated order; sort only directory scans. */
    if (boards->count == 0) {
        scan_boards(boards, dir);
        qsort(boards->items, (size_t)boards->count, sizeof(ThemeEntry), cmp_entry);
    }
    if (pieces->count == 0) {
        scan_pieces(pieces, dir);
        qsort(pieces->items, (size_t)pieces->count, sizeof(ThemeEntry), cmp_entry);
    }

    add_builtins(boards, pieces);
    return boards->count > 0 && pieces->count > 0;
}

void themes_free(ThemeList *list)
{
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

int themes_index_of(const ThemeList *list, const char *key)
{
    if (!list || !key) return -1;
    for (int i = 0; i < list->count; i++)
        if (strcmp(list->items[i].key, key) == 0) return i;
    return -1;
}
