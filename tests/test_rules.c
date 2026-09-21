#include "../src/board.h"
#include "../src/move.h"
#include "../src/pgn.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void play(Board *b, const char *san)
{
    Board before = *b;
    if (!san_apply(b, san)) {
        printf("FAIL  illegal/unparsed move '%s'\n", san);
        board_print(&before);
        failures++;
    }
}

static void test_scholar()
{
    Board b;
    board_reset(&b);
    play(&b, "e4");
    play(&b, "e5");
    play(&b, "Bc4");
    play(&b, "Nc6");
    play(&b, "Qh5");
    play(&b, "Nf6");
    Board before = b;
    Move m;
    CHECK(san_find(&b, "Qxf7#", &m));
    char san[16];
    move_to_san(&before, m, san, sizeof san);
    CHECK(strcmp(san, "Qxf7#") == 0);
    play(&b, "Qxf7#");
    CHECK(game_state(&b) == CHECKMATE);
    printf("scholar's mate  ok (san='%s')\n", san);
}

static void test_kiwipete()
{
    /* Kiwipete has both-side casting. Start position covers castling rights.
       Verify initial legal move count and no game-over. */
    Board b;
    board_reset(&b);
    CHECK(game_state(&b) == NO_GAME_OVER);
    MoveList ml;
    gen_legal(&b, &ml);
    CHECK(ml.count == 20);
    printf("start legal moves=%d  ok\n", ml.count);
}

static void test_en_passant()
{
    Board b;
    board_reset(&b);
    play(&b, "e4");   /* w e4 */
    play(&b, "d5");   /* b d5 (not double, ok) */
    play(&b, "e5");   /* w e5 */
    play(&b, "f5");   /* b double push f7-f5 -> ep on f6 */
    CHECK(b.ep_square == algebraic_to_sq("f6"));
    play(&b, "exf6"); /* en passant capture */
    /* e.p. should remove black pawn on f5 */
    CHECK(board_get(&b, algebraic_to_sq("f5")) == EMPTY);
    printf("en passant  ok\n");
}

static void test_castling()
{
    Board b;
    board_reset(&b);
    play(&b, "e4");
    play(&b, "e5");
    play(&b, "Nf3");
    play(&b, "Nc6");
    play(&b, "Bc4");
    play(&b, "Bc5");
    play(&b, "O-O");
    CHECK(board_get(&b, 6) == WK);
    CHECK(board_get(&b, 5) == WR);
    printf("kingside castle  ok\n");

    /* queenside */
    Board b2;
    board_reset(&b2);
    play(&b2, "d4");
    play(&b2, "d5");
    play(&b2, "Nc3");
    play(&b2, "Nc6");
    play(&b2, "Bf4");
    play(&b2, "Bf5");
    play(&b2, "Qd2");
    play(&b2, "Qd7");
    play(&b2, "O-O-O");
    CHECK(board_get(&b2, 2) == WK);
    CHECK(board_get(&b2, 3) == WR);
    printf("queenside castle  ok\n");
}

static void test_promotion()
{
    Board b;

    /* white promotes capturing on rank 8. */
    memset(&b, 0, sizeof b);
    b.side = WHITE;
    b.castling = 0;
    b.board[algebraic_to_sq("g7")] = WP;
    b.board[algebraic_to_sq("f8")] = BR;
    b.board[algebraic_to_sq("h8")] = BK;
    b.board[algebraic_to_sq("g1")] = WK;

    Board t = b;
    play(&t, "gxf8=Q");
    CHECK(board_get(&t, algebraic_to_sq("f8")) == WQ);
    printf("white promotion capture->Q  ok\n");

    /* black promotion */
    memset(&b, 0, sizeof b);
    b.side = BLACK;
    b.castling = 0;
    b.board[algebraic_to_sq("a2")] = BP;
    b.board[algebraic_to_sq("b1")] = WN;
    b.board[algebraic_to_sq("a1")] = BK;
    b.board[algebraic_to_sq("f8")] = WK;

    t = b;
    play(&t, "axb1=N");
    CHECK(board_get(&t, algebraic_to_sq("b1")) == BN);
    printf("black promotion to N  ok\n");

    /* plain queen (no suffix) */
    memset(&b, 0, sizeof b);
    b.side = WHITE;
    b.castling = 0;
    b.board[algebraic_to_sq("h7")] = WP;
    b.board[algebraic_to_sq("a8")] = BK;
    b.board[algebraic_to_sq("g1")] = WK;
    t = b;
    play(&t, "h8");
    CHECK(board_get(&t, algebraic_to_sq("h8")) == WQ);
    printf("default promotion to Q  ok\n");
}

static void test_illegal_self_check()
{
    Board b;
    memset(&b, 0, sizeof b);
    b.side = WHITE;
    b.castling = 0;
    b.board[algebraic_to_sq("f2")] = WP;   /* pinned pawn */
    b.board[algebraic_to_sq("e1")] = WK;
    b.board[algebraic_to_sq("h4")] = BB;   /* attacks e1 diagonally */
    b.board[algebraic_to_sq("h8")] = BK;
    MoveList ml;
    gen_legal(&b, &ml);
    int pawn_moves = 0;
    for (int i = 0; i < ml.count; i++)
        if (MOVE_FROM(ml.moves[i]) == algebraic_to_sq("f2")) pawn_moves++;
    CHECK(pawn_moves == 0);
    printf("pinned pawn  ok\n");
}

static void test_san_roundtrip()
{
    Board b;
    board_reset(&b);

    const char *moves[] = {
        "e4", "e5", "Nf3", "Nc6", "Bb5", "a6",
        "Ba4", "Nf6", "O-O", "Be7", "Re1", "b5",
        "Bb3", "O-O", "c3", "d5", "exd5", "Nxd5",
        "Nxe5", "Nxe5", "Rxe5", "c6", NULL
    };
    for (int i = 0; moves[i]; i++) {
        if (!san_apply(&b, moves[i])) {
            printf("FAIL san roundtrip move '%s' (ply %d)\n", moves[i], i);
            failures++;
            break;
        }
    }
    printf("SAN roundtrip sequence  ok\n");
}

static void test_move_to_san_ascii()
{
    Board b;
    board_reset(&b);
    char out[16];
    MoveList legal;
    gen_legal(&b, &legal);
    Move m = 0;
    for (int i = 0; i < legal.count; i++) {
        if (MOVE_TO(legal.moves[i]) == algebraic_to_sq("e4") &&
            b.board[MOVE_FROM(legal.moves[i])] == WP) {
            m = legal.moves[i]; break;
        }
    }
    move_to_san(&b, m, out, sizeof out);
    CHECK(strcmp(out, "e4") == 0);
    printf("move_to_san e4 = '%s'  ok\n", out);
}

int main(void)
{
    test_scholar();
    test_kiwipete();
    test_en_passant();
    test_castling();
    test_promotion();
    test_illegal_self_check();
    test_san_roundtrip();
    test_move_to_san_ascii();

    if (failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}