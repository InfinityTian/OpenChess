#ifndef MOVETREE_H
#define MOVETREE_H

#include "board.h"
#include "move.h"
#include <stdbool.h>

/*
 * Move tree: the mainline plus variations. A synthetic root holds the starting
 * position; each child is a move. Siblings are alternatives (variations).
 */

typedef struct MoveNode {
    Move  move;                 /* the move leading here (0 for the root) */
    char  san[8];
    Board board;                /* position AFTER the move */
    int   nag;                  /* PGN NAG, 0 = none */
    int   cls;                  /* review classification (ReviewClass) */
    int   eval_cp;              /* review eval (side-to-move perspective) */
    int   eval_mate;
    bool  has_eval;
    char  comment[96];

    struct MoveNode *parent;
    struct MoveNode *first, *last;   /* children */
    struct MoveNode *prev, *next;    /* siblings */
} MoveNode;

/* Create a synthetic root at position `start`. */
MoveNode *mt_new_root(const Board *start);

/* Add a child move to `parent` (applies it; computes SAN when `san` is NULL). */
MoveNode *mt_add_child(MoveNode *parent, Move m, const char *san);

/* Find an existing child of `parent` playing the same move, or NULL. */
MoveNode *mt_find_child(const MoveNode *parent, Move m);

/* Deep-copy `src` and its whole subtree (children, recursively). */
MoveNode *mt_copy(const MoveNode *src);

/*
 * Merge the tree `src` into `dst` (both rooted at the same position): shared
 * prefixes are reused and their continuations combined, and any new move is
 * added with its complete subtree, so all children/siblings/variations survive.
 * Returns the number of nodes added.
 */
int mt_merge(MoveNode *dst, const MoveNode *src);

/* Depth (plies from root) and whether the node has sibling variations. */
int  mt_depth(const MoveNode *n);
int  mt_sibling_count(const MoveNode *n);

/* Rebuild the linear arrays for the path root..`node` (for the move list and
 * PGN export). Caps at `max`; returns the number of plies written. */
int mt_fill_path(const MoveNode *node, Board before[], Move history[],
                 char sans[][8], int max);

void mt_free(MoveNode *root);

#endif
