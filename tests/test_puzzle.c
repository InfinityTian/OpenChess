#include "../src/puzzle.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void)
{
    const char *path = "/tmp/oc_puzzles_test.jsonl";
    FILE *f = fopen(path, "w");
    if (!f) { printf("SKIP: cannot write %s\n", path); return 0; }
    fputs("{\"id\":\"a1\",\"fen\":\"8/8/8/8/8/8/8/K6k w - - 0 1\","
          "\"moves\":\"a1a2 h1h2\",\"rating\":1200,"
          "\"themes\":[\"fork\",\"middlegame\"]}\n", f);
    fputs("{\"id\":\"a2\",\"fen\":\"8/8/8/8/8/8/8/K6k w - - 0 1\","
          "\"moves\":\"a1b1\",\"rating\":1800,\"themes\":[\"pin\"]}\n", f);
    fputs("{\"id\":\"a3\",\"fen\":\"8/8/8/8/8/8/8/K6k w - - 0 1\","
          "\"moves\":\"h1g2\",\"rating\":2200,"
          "\"themes\":[\"endgame\",\"fork\"]}\n", f);
    fputs("not json\n", f);
    fclose(f);

    Puzzles *p = puzzles_load(path);
    CHECK(p != NULL);
    if (!p) { remove(path); return 1; }

    CHECK(puzzles_count(p) == 3);

    Puzzle z;
    CHECK(puzzles_get(p, 0, &z) && strcmp(z.id, "a1") == 0);
    CHECK(z.rating == 1200);
    CHECK(strstr(z.themes, "fork") != NULL);
    CHECK(puzzles_get(p, 5, &z) == false);

    int got = 0;
    for (int i = 0; i < 40; i++) {
        if (puzzles_pick(p, 1000, 1300, NULL, &z)) {
            CHECK(z.rating >= 1000 && z.rating <= 1300);
            got = 1;
        }
    }
    CHECK(got);
    CHECK(puzzles_pick(p, 1000, 1300, "pin", &z) == false);
    CHECK(puzzles_pick(p, 1000, 2400, "fork", &z) == true);
    CHECK(puzzles_pick(p, 3000, 4000, NULL, &z) == false);

    puzzles_free(p);
    remove(path);

    /* missing file -> NULL */
    CHECK(puzzles_load("/tmp/oc_puzzles_missing.jsonl") == NULL);

    if (failures == 0) {
        printf("puzzle loader  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
