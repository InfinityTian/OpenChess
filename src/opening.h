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
/* Opening name for the exact position, when it is on a known line. */
bool         opening_name_at(const OpeningBook *b, const Board *pos,
                             char *out, size_t n);

/* ECO code + name for the exact position (deepest book line through it). */
bool         opening_pos_info(const OpeningBook *b, const Board *pos,
                              char *eco, size_t eco_n,
                              char *name, size_t name_n);

/*
 * Indices of every book line whose position set contains `pos` (so different
 * move orders that reach the same position all match). Returns the count.
 */
int          opening_lines_at(const OpeningBook *b, const Board *pos,
                              int *out, int max);

/*
 * The SAN move that line `index` plays right after `pos` (which may occur at any
 * ply in the line). Returns 1 and writes it, 0 if `pos` is the end of the line,
 * or -1 if the position is not on the line.
 */
int          opening_line_next(const OpeningBook *b, int index, const Board *pos,
                               char *out, size_t n);

/*
 * Recognize the opening of a played line: `sans` holds `n` SAN moves from the
 * initial position. Matches the line whose move sequence shares the longest
 * prefix with the played path (the path may run past the book, or the book line
 * may be longer). Writes the line's ECO and name and returns the number of
 * matched plies, or 0 when nothing matches.
 */
int          opening_lookup(const OpeningBook *b, const char (*sans)[8], int n,
                            char *eco, size_t eco_n, char *name, size_t name_n);

int          opening_line_count(const OpeningBook *b);
bool opening_line_at(const OpeningBook *b, int index,
                     char *eco, size_t eco_n, char *name, size_t name_n,
                     char *moves, size_t moves_n);

#endif
