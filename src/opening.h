#ifndef OPENING_H
#define OPENING_H

#include "board.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Tiny opening book: a set of positions reachable by the opening lines in a TSV
 * (e.g. lichess-org/chess-openings: eco<TAB>name<TAB>pgn). A position is "book"
 * when it appears on one of those lines.
 */

typedef struct OpeningBook OpeningBook;

OpeningBook *opening_load(const char *path);   /* NULL when missing/empty */
void         opening_free(OpeningBook *b);
int          opening_count(const OpeningBook *b);
bool         opening_is_book(const OpeningBook *b, const Board *pos);

/* Browse support: the raw lines (eco / name / SAN movetext). */
int  opening_line_count(const OpeningBook *b);
bool opening_line_at(const OpeningBook *b, int index,
                     char *eco, size_t eco_n, char *name, size_t name_n,
                     char *moves, size_t moves_n);

#endif
