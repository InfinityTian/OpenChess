#include "movetree.h"
#include "pgn.h"
#include <stdlib.h>
#include <string.h>

MoveNode *mt_new_root(const Board *start)
{
    MoveNode *r = calloc(1, sizeof *r);
    if (!r) return NULL;
    if (start) r->board = *start;
    return r;
}

MoveNode *mt_add_child(MoveNode *parent, Move m, const char *san)
{
    if (!parent) return NULL;
    MoveNode *n = calloc(1, sizeof *n);
    if (!n) return NULL;

    n->move = m;
    n->parent = parent;
    n->board = parent->board;
    if (san) {
        snprintf(n->san, sizeof n->san, "%s", san);
    } else {
        move_to_san(&parent->board, m, n->san, sizeof n->san);
    }
    make_move_plumb(&n->board, m);

    n->prev = parent->last;
    if (parent->last) parent->last->next = n;
    else parent->first = n;
    parent->last = n;
    return n;
}

MoveNode *mt_find_child(const MoveNode *parent, Move m)
{
    if (!parent) return NULL;
    for (MoveNode *c = parent->first; c; c = c->next)
        if (MOVE_FROM(c->move) == MOVE_FROM(m) &&
            MOVE_TO(c->move) == MOVE_TO(m) &&
            (MOVE_FLAGS(c->move) & FLAG_PROMO) == (MOVE_FLAGS(m) & FLAG_PROMO) &&
            (!(MOVE_FLAGS(m) & FLAG_PROMO) || c->move == m))
            return c;
    return NULL;
}

static MoveNode *copy_subtree(const MoveNode *src)
{
    MoveNode *n = calloc(1, sizeof *n);
    if (!n) return NULL;
    n->move = src->move;
    snprintf(n->san, sizeof n->san, "%s", src->san);
    n->board = src->board;
    n->nag = src->nag;
    n->cls = src->cls;
    n->eval_cp = src->eval_cp;
    n->eval_mate = src->eval_mate;
    n->has_eval = src->has_eval;
    snprintf(n->comment, sizeof n->comment, "%s", src->comment);

    MoveNode *prev = NULL;
    for (const MoveNode *c = src->first; c; c = c->next) {
        MoveNode *cc = copy_subtree(c);
        if (!cc) break;
        cc->parent = n;
        cc->prev = prev;
        cc->next = NULL;
        if (prev) prev->next = cc;
        else      n->first = cc;
        prev = cc;
    }
    n->last = prev;
    return n;
}

MoveNode *mt_copy(const MoveNode *src)
{
    return src ? copy_subtree(src) : NULL;
}

static int subtree_nodes(const MoveNode *n)
{
    int c = 1;
    for (const MoveNode *k = n->first; k; k = k->next) c += subtree_nodes(k);
    return c;
}

int mt_merge(MoveNode *dst, const MoveNode *src)
{
    if (!dst || !src) return 0;
    int added = 0;
    for (const MoveNode *c = src->first; c; c = c->next) {
        MoveNode *match = mt_find_child(dst, c->move);
        if (match) {
            added += mt_merge(match, c);
            continue;
        }
        MoveNode *copy = copy_subtree(c);
        if (!copy) continue;
        copy->parent = dst;
        copy->prev = dst->last;
        copy->next = NULL;
        if (dst->last) dst->last->next = copy;
        else           dst->first = copy;
        dst->last = copy;
        added += subtree_nodes(copy);
    }
    return added;
}

int mt_depth(const MoveNode *n)
{
    int d = 0;
    for (; n && n->parent; n = n->parent) d++;
    return d;
}

int mt_sibling_count(const MoveNode *n)
{
    if (!n) return 0;
    int c = 0;
    for (const MoveNode *s = n->parent ? n->parent->first : NULL; s; s = s->next)
        c++;
    return c;
}

int mt_fill_path(const MoveNode *node, Board before[], Move history[],
                 char sans[][8], int max)
{
    /* collect nodes root->node */
    const MoveNode *chain[1024];
    int d = 0;
    for (const MoveNode *n = node; n && n->parent && d < 1024; n = n->parent)
        chain[d++] = n;
    if (d > max) d = max;

    for (int i = 0; i < d; i++) {
        const MoveNode *n = chain[d - 1 - i];
        before[i] = n->parent->board;
        history[i] = n->move;
        snprintf(sans[i], 8, "%s", n->san);
    }
    return d;
}

void mt_free(MoveNode *root)
{
    if (!root) return;
    for (MoveNode *c = root->first; c; ) {
        MoveNode *nx = c->next;
        mt_free(c);
        c = nx;
    }
    free(root);
}
