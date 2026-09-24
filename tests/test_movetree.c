#include "../src/movetree.h"
#include "../src/pgn.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static MoveNode *add(MoveNode *parent, const char *san)
{
    Move m;
    if (!san_find(&parent->board, san, &m)) return NULL;
    return mt_add_child(parent, m, NULL);
}

static MoveNode *child(MoveNode *parent, const char *san)
{
    for (MoveNode *c = parent ? parent->first : NULL; c; c = c->next)
        if (strcmp(c->san, san) == 0) return c;
    return NULL;
}

static int count_children(const MoveNode *n)
{
    int c = 0;
    for (const MoveNode *k = n ? n->first : NULL; k; k = k->next) c++;
    return c;
}

int main(void)
{
    Board start;
    board_reset(&start);
    MoveNode *t1 = mt_new_root(&start);
    MoveNode *t2 = mt_new_root(&start);

    /* tree 1: 1. e4 e5 2. Nf3 Nc6 3. Bb5 */
    MoveNode *a = add(t1, "e4");
    MoveNode *a1 = add(a, "e5");
    MoveNode *a2 = add(a1, "Nf3");
    MoveNode *a3 = add(a2, "Nc6");
    add(a3, "Bb5");

    /* tree 2: 1. e4 e5 2. Nf3 Nf6  and the alternative 1... c5 */
    MoveNode *b = add(t2, "e4");
    MoveNode *b1 = add(b, "e5");
    MoveNode *b2 = add(b1, "Nf3");
    add(b2, "Nf6");
    add(b, "c5");

    /* merge t2 into t1 */
    int added = mt_merge(t1, t2);
    CHECK(added > 0);

    /* shared root move e4 is reused, and both replies are siblings */
    CHECK(t1->first && strcmp(t1->first->san, "e4") == 0);
    CHECK(count_children(t1) == 1);                 /* only one e4 */
    CHECK(count_children(a) == 2);                  /* e5 and c5 */
    CHECK(child(a, "e5") && child(a, "c5"));
    CHECK(child(a, "e5") == a1);                    /* original node reused */

    /* e5 -> Nf3 keeps its continuation and gains the Nf6 sibling */
    CHECK(count_children(a2) == 2);                 /* Nc6 and Nf6 */
    CHECK(child(a2, "Nc6") == a3 && child(a2, "Nf6"));
    CHECK(a3->first && strcmp(a3->first->san, "Bb5") == 0);   /* preserved */

    /* find/child + deep copy keep the whole subtree */
    CHECK(mt_find_child(a, a1->move) != NULL);      /* e5 already present -> reused */
    CHECK(mt_find_child(a1, a2->move) != NULL);
    MoveNode *copy = mt_copy(a);
    CHECK(copy && strcmp(copy->san, "e4") == 0);
    CHECK(count_children(copy) == 2);
    CHECK(copy->first != a->first);                 /* a real copy */
    mt_free(copy);

    printf("move tree merge (siblings + subtrees)  ok\n");

    mt_free(t1);
    mt_free(t2);
    if (failures == 0) { printf("\nALL TESTS PASSED\n"); return 0; }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
