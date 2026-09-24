#include "../src/gui.h"
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Two Gui instances talk over a real loopback TCP connection: one hosts and
 * plays White, the other joins as Black, and a move is propagated. */

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void nap(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void send_key(Gui *g, SDL_Keycode k)
{
    SDL_Event e = {0};
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = k;
    gui_handle_event(g, &e);
}

static void click_sq(Gui *g, const char *alg, bool down)
{
    int sq = algebraic_to_sq(alg);
    int file = sq % 8, rank = sq / 8;
    int col = g->flipped ? 7 - file : file;
    int row = g->flipped ? rank : 7 - rank;
    SDL_Event e = {0};
    e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = BOARD_X + col * SQ_SIZE + SQ_SIZE / 2;
    e.button.y = BOARD_Y + row * SQ_SIZE + SQ_SIZE / 2;
    gui_handle_event(g, &e);
}

int main(void)
{
    SDL_Init(0);
    if (!net_init() || !net_available()) {
        printf("SKIP: SDL_net unavailable\n");
        return 0;
    }

    Gui *host = gui_create();
    Gui *join = gui_create();
    CHECK(host && join);

    const char *port = "45812";

    /* host: menu -> Local Multiplayer -> Host */
    host->menu_index = 3;
    send_key(host, SDLK_RETURN);
    CHECK(host->scene == SCENE_HOSTJOIN);
    snprintf(host->net_port, sizeof host->net_port, "%s", port);
    host->hj_focus = 2;
    send_key(host, SDLK_RETURN);

    /* join: menu -> Local Multiplayer -> Join */
    join->menu_index = 3;
    send_key(join, SDLK_RETURN);
    snprintf(join->net_addr, sizeof join->net_addr, "127.0.0.1");
    snprintf(join->net_port, sizeof join->net_port, "%s", port);
    join->hj_focus = 3;
    send_key(join, SDLK_RETURN);

    CHECK(join->scene == SCENE_GAME);
    CHECK(join->mode == MODE_LOCAL);
    CHECK(join->local_color == BLACK);

    Uint32 t = SDL_GetTicks();
    for (int i = 0; i < 1000 && host->scene != SCENE_GAME; i++) {
        gui_tick(host, t);
        gui_tick(join, t);
        t += 5;
        nap(5);
    }
    CHECK(host->scene == SCENE_GAME);
    CHECK(host->mode == MODE_LOCAL);
    CHECK(host->local_color == WHITE);

    /* host (White) plays e2e4 through the board */
    click_sq(host, "e2", true);
    click_sq(host, "e4", false);
    CHECK(host->ply == 1);
    CHECK(host->board.board[algebraic_to_sq("e4")] == WP);

    /* publish it, then let the peer receive it */
    for (int i = 0; i < 1000 && join->ply == 0; i++) {
        gui_tick(host, t);
        gui_tick(join, t);
        t += 5;
        nap(5);
    }
    CHECK(host->net_sent_ply == 1);
    CHECK(join->ply == 1);
    CHECK(join->board.board[algebraic_to_sq("e4")] == WP);
    CHECK(join->board.side == BLACK);
    CHECK(join->flipped);           /* auto-oriented to Black at the bottom */

    /* black (join) replies e7e5 and it propagates back to the host */
    click_sq(join, "e7", true);
    click_sq(join, "e5", false);
    CHECK(join->ply == 2);

    for (int i = 0; i < 1000 && host->ply < 2; i++) {
        gui_tick(host, t);
        gui_tick(join, t);
        t += 5;
        nap(5);
    }
    CHECK(host->ply == 2);
    CHECK(host->board.board[algebraic_to_sq("e5")] == BP);
    CHECK(host->board.side == WHITE);

    gui_destroy(host);
    gui_destroy(join);
    net_shutdown();
    SDL_Quit();

    if (failures == 0) {
        printf("local multiplayer loopback move  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
