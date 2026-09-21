#include "../src/board.h"
#include "../src/fen.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static bool boards_equal(const Board *a, const Board *b)
{
    if (memcmp(a->board, b->board, sizeof a->board) != 0) return false;
    if (a->side != b->side) return false;
    if (a->castling != b->castling) return false;
    if (a->ep_square != b->ep_square) return false;
    if (a->halfmove_clock != b->halfmove_clock) return false;
    if (a->fullmove_number != b->fullmove_number) return false;
    return true;
}

static void test_start(void)
{
    Board b;
    board_reset(&b);
    char out[128];
    fen_generate(&b, out, sizeof out);
    CHECK(strcmp(out, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") == 0);

    Board p;
    CHECK(fen_parse(out, &p));
    CHECK(boards_equal(&b, &p));
    printf("startpos roundtrip  ok ('%s')\n", out);
}

static void test_kiwipete(void)
{
    const char *fen =
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    Board b;
    CHECK(fen_parse(fen, &b));
    char out[128];
    fen_generate(&b, out, sizeof out);
    CHECK(strcmp(out, fen) == 0);
    printf("kiwipete roundtrip  ok\n");
}

static void test_en_passant_and_clocks(void)
{
    const char *fen = "rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 3 7";
    Board b;
    CHECK(fen_parse(fen, &b));
    CHECK(b.ep_square == algebraic_to_sq("d6"));
    CHECK(b.halfmove_clock == 3);
    CHECK(b.fullmove_number == 7);
    char out[128];
    fen_generate(&b, out, sizeof out);
    CHECK(strcmp(out, fen) == 0);
    printf("en passant + clocks  ok\n");
}

static void test_castling_bits(void)
{
    Board b;
    CHECK(fen_parse("4k3/8/8/8/8/8/8/4K3 w Kq - 0 1", &b));
    CHECK(b.castling == 0x9);           /* K (0x1) | q (0x8) */
    CHECK(fen_parse("4k3/8/8/8/8/8/8/4K3 b - - 0 1", &b));
    CHECK(b.castling == 0);
    CHECK(b.side == BLACK);
    printf("castling bits + side  ok\n");
}

static void test_optional_clocks(void)
{
    Board b;
    CHECK(fen_parse("4k3/8/8/8/8/8/8/4K3 w - -", &b));
    CHECK(b.halfmove_clock == 0);
    CHECK(b.fullmove_number == 1);
    printf("optional clocks  ok\n");
}

static void test_invalid(void)
{
    Board b;
    CHECK(!fen_parse(NULL, &b));
    CHECK(!fen_parse("", &b));
    CHECK(!fen_parse("not a fen", &b));
    CHECK(!fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1", &b));
    CHECK(!fen_parse("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w Z - 0 1", &b));
    /* missing white king */
    CHECK(!fen_parse("4k3/8/8/8/8/8/8/8 w - - 0 1", &b));
    /* extra rank data */
    CHECK(!fen_parse("4k3/8/8/8/8/8/8/4K3/8 w - - 0 1", &b));
    /* trailing junk */
    CHECK(!fen_parse("4k3/8/8/8/8/8/8/4K3 w - - 0 1 junk", &b));
    printf("invalid inputs rejected  ok\n");
}

int main(void)
{
    test_start();
    test_kiwipete();
    test_en_passant_and_clocks();
    test_castling_bits();
    test_optional_clocks();
    test_invalid();

    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
