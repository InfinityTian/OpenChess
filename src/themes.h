#ifndef THEMES_H
#define THEMES_H

#include <stdbool.h>
#include <stddef.h>

#define THEME_KEY_MAX   64
#define THEME_LABEL_MAX 64
#define THEME_PATH_MAX  1024

/*
 * One selectable board or piece theme.
 *
 * Boards: `path` is a PNG file; when empty the board is drawn procedurally from
 * `light`/`dark` (a builtin fallback theme).
 * Pieces: `path` is a directory of wp/wn/.../bk.png; when empty the glyph font
 * is used (the builtin fallback).
 */
typedef struct {
    char key[THEME_KEY_MAX];
    char label[THEME_LABEL_MAX];
    char path[THEME_PATH_MAX];
    bool builtin;
    unsigned char light[3];
    unsigned char dark[3];
    unsigned char coord[3];
} ThemeEntry;

typedef struct {
    ThemeEntry *items;
    int count;
} ThemeList;

/*
 * Populate `boards` and `pieces` from <assets_dir>/themes.txt, cross-checked
 * against PNGs in the boards directory and piece-set subdirectories. Missing
 * manifest entries fall back to a directory scan, and always-present builtin
 * (procedural board / glyph piece) themes are appended.
 *
 * Returns true when at least one board and one piece theme are available.
 */
bool themes_load(ThemeList *boards, ThemeList *pieces, const char *assets_dir);

void themes_free(ThemeList *list);

/* Index of the entry with `key`, or -1. */
int themes_index_of(const ThemeList *list, const char *key);

#endif
