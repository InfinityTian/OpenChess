#include "../src/board.h"
#include "../src/move.h"
#include "../src/pgn.h"
#include "../src/fen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool file_contains(FILE *f, const char *needle)
{
    rewind(f);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

static void test_write(void)
{
    const char *san[] = { "e4", "e5", "Nf3", "Nc6", "Bb5", "a6" };
    char moves[6][8];
    for (int i = 0; i < 6; i++) snprintf(moves[i], sizeof moves[i], "%s", san[i]);

    FILE *f = tmpfile();
    CHECK(f != NULL);
    if (!f) return;

    pgn_write(f, (const char (*)[8])moves, 6,
              "Test Event", "Test Site", "2026.01.01", 1,
              "Alice", "Bob", "*");

    CHECK(file_contains(f, "[Event \"Test Event\"]"));
    CHECK(file_contains(f, "[White \"Alice\"]"));
    CHECK(file_contains(f, "[Black \"Bob\"]"));
    CHECK(file_contains(f, "[Result \"*\"]"));
    CHECK(file_contains(f, "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 *"));
    fclose(f);
    printf("pgn_write headers + movetext  ok\n");
}

static void test_result(void)
{
    Board b;

    CHECK(fen_parse("r1bqkb1r/pppp1Qpp/2n2n2/4p3/2B1P3/8/PPPP1PPP/RNB1K1NR b KQkq - 0 4", &b));
    CHECK(game_state(&b) == CHECKMATE);
    CHECK(strcmp(pgn_result(&b, CHECKMATE), "1-0") == 0);

    CHECK(fen_parse("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", &b));
    CHECK(game_state(&b) == STALEMATE);
    CHECK(strcmp(pgn_result(&b, STALEMATE), "1/2-1/2") == 0);

    CHECK(fen_parse("8/8/8/8/8/8/8/K6k w - - 0 1", &b));
    CHECK(strcmp(pgn_result(&b, NO_GAME_OVER), "*") == 0);
    printf("pgn_result tokens  ok\n");
}

static void test_import(void)
{
    const char *pgn =
        "[Event \"T\"]\n[White \"A\"]\n[Black \"B\"]\n\n"
        "1. e4 (1. d4 d5) e5 2. Nf3 {good} Nc6 $1 3. Bb5 a6 1/2-1/2\n";

    PgnHeaders h;
    MoveNode *root = pgn_parse_text(pgn, &h);
    CHECK(root != NULL);
    CHECK(strcmp(h.event, "T") == 0);
    CHECK(strcmp(h.white, "A") == 0);
    CHECK(strcmp(h.result, "1/2-1/2") == 0);

    CHECK(root->first && strcmp(root->first->san, "e4") == 0);
    /* variation sibling of e4 */
    MoveNode *var = root->first ? root->first->next : NULL;
    CHECK(var && strcmp(var->san, "d4") == 0);
    CHECK(var && var->first && strcmp(var->first->san, "d5") == 0);
    CHECK(var && var->next == NULL);
    /* mainline continues */
    CHECK(root->first->first && strcmp(root->first->first->san, "e5") == 0);

    /* comment + NAG */
    MoveNode *nf3 = root->first->first->first;
    CHECK(nf3 && strcmp(nf3->san, "Nf3") == 0);
    CHECK(nf3 && strcmp(nf3->comment, "good") == 0);
    MoveNode *nc6 = nf3 ? nf3->first : NULL;
    CHECK(nc6 && strcmp(nc6->san, "Nc6") == 0);
    CHECK(nc6 && nc6->nag == 1);

    char *out = pgn_serialize(root, &h);
    CHECK(out != NULL);
    if (out) {
        CHECK(strstr(out, "(1. d4 d5") != NULL);
        CHECK(strstr(out, "{good}") != NULL);
        CHECK(strstr(out, "Nc6 $1") != NULL);
        CHECK(strstr(out, "1/2-1/2") != NULL);
        free(out);
    }
    mt_free(root);
    printf("pgn import/export variations  ok\n");
}

/* Two pawns can capture the same square: disambiguation must not add a rank. */
static void test_pawn_disambiguation(void)
{
    Board b;
    CHECK(fen_parse("6k1/8/8/8/6p1/5P1P/8/6K1 w - - 0 1", &b));
    Move m;
    char san[16];
    CHECK(uci_to_move(&b, "h3g4", &m));
    move_to_san(&b, m, san, sizeof san);
    CHECK(strcmp(san, "hxg4") == 0);
    CHECK(uci_to_move(&b, "f3g4", &m));
    move_to_san(&b, m, san, sizeof san);
    CHECK(strcmp(san, "fxg4") == 0);
    printf("pawn capture SAN  ok\n");
}

/* Two knights reach the same square from different files and ranks. */
static void test_knight_disambiguation(void)
{
    Board b;
    CHECK(fen_parse("4k3/8/8/8/8/8/4N3/1N4K1 w - - 0 1", &b));
    Move m;
    char san[16];
    CHECK(uci_to_move(&b, "b1c3", &m));
    move_to_san(&b, m, san, sizeof san);
    CHECK(strcmp(san, "Nbc3") == 0);
    CHECK(uci_to_move(&b, "e2c3", &m));
    move_to_san(&b, m, san, sizeof san);
    CHECK(strcmp(san, "Nec3") == 0);
    printf("knight SAN disambiguation  ok\n");
}

/* Round-trip a real chess.com PGN when present (regression for bad SAN/NAGs). */
static void test_chesscom_file(void)
{
    const char *p = "/Users/infinitytian/Downloads/"
                    "Coach-Dante_vs_InfinityTian_2026.05.25.pgn";
    FILE *f = fopen(p, "r");
    if (!f) { printf("(chess.com sample not found; skipped)\n"); return; }
    static char txt[131072];
    size_t n = fread(txt, 1, sizeof txt - 1, f);
    txt[n] = '\0';
    fclose(f);

    PgnHeaders h;
    MoveNode *root = pgn_parse_text(txt, &h);
    char *out = pgn_serialize(root, &h);
    CHECK(out != NULL);
    if (out) {
        CHECK(strstr(out, "hhxg4") == NULL);
        CHECK(strstr(out, "hxg4") != NULL);
        CHECK(strstr(out, "(17. Nxc7") != NULL);
        CHECK(strstr(out, " $1") != NULL);
        free(out);
    }
    mt_free(root);
    printf("chess.com PGN round-trip  ok\n");
}

int main(void)
{
    test_write();
    test_result();
    test_import();
    test_pawn_disambiguation();
    test_knight_disambiguation();
    test_chesscom_file();

    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
