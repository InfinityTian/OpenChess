#ifndef PUZZLE_H
#define PUZZLE_H

#include <stdbool.h>

/*
 * Lichess puzzle trainer data. The importer (scripts/import_puzzles.py) writes
 * a small JSONL subset; this loader indexes it in memory and picks puzzles by
 * rating band and theme.
 */

typedef struct {
    char id[16];
    char fen[80];
    char moves[256];   /* space-separated UCI; move 0 is the opponent's */
    int  rating;
    char themes[192];  /* '|'-separated for easy substring matching */
} Puzzle;

typedef struct Puzzles Puzzles;

/* Load a JSONL puzzle file. Returns NULL when the file is missing/empty. */
Puzzles *puzzles_load(const char *path);
void     puzzles_free(Puzzles *p);
int      puzzles_count(const Puzzles *p);

/* Copy the puzzle at an index (0-based). */
bool puzzles_get(const Puzzles *p, int index, Puzzle *out);

/* Pick a random puzzle within [rmin,rmax] whose themes contain `theme`
 * (NULL/empty = any). false when no puzzle matches. */
bool puzzles_pick(const Puzzles *p, int rmin, int rmax, const char *theme,
                  Puzzle *out);

#endif
