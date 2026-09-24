#include "../src/gui.h"
#include "../src/pgn.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Headless smoke test: create the GUI offscreen, play some moves, render a
 * frame, and save a BMP. Exits nonzero on any SDL/font/render failure. */

/* Count sampled pixels in the region that differ from `bg`. */
static int count_non_bg(SDL_Renderer *ren, int x0, int y0, int w, int h, Uint32 bg)
{
    SDL_Surface *s = SDL_CreateRGBSurface(0, w, h, 32,
                                          0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (!s) return -1;
    if (SDL_RenderReadPixels(ren, &(SDL_Rect){ x0, y0, w, h },
                             SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) != 0) {
        SDL_FreeSurface(s);
        return -1;
    }
    Uint32 mapped = SDL_MapRGBA(s->format,
                                (bg >> 16) & 0xff, (bg >> 8) & 0xff, bg & 0xff, 255);
    int n = 0;
    for (int y = 0; y < h; y += 2)
        for (int x = 0; x < w; x += 2) {
            Uint32 *px = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch + x * 4);
            if (*px != mapped) n++;
        }
    SDL_FreeSurface(s);
    return n;
}

static void do_restart_test(Gui *g)
{
    board_reset(&g->board);
    g->ply = 0;
    g->selected = -1;
    g->promo_from = g->promo_to = -1;
    g->input.len = 0;
    g->msg[0] = 0;
    g->state = NO_GAME_OVER;
}

/* Engine settings must survive a save/load round-trip. */
static int test_engine_config(void)
{
    Gui *a = gui_create();
    if (!a) return 1;
    a->eng_multipv = 3;
    a->eng_threads = 4;
    a->eng_hash = 128;
    a->eng_time_ms = 500;
    a->eng_depth = 12;
    a->sound = false;
    a->max_fps = 144;
    a->config_dirty = true;
    snprintf(a->config_path, sizeof a->config_path, "/tmp/oc_engine_test.conf");
    gui_save_config(a);
    gui_destroy(a);

    Gui *b = gui_create();
    if (!b) return 1;
    gui_load_config(b, "/tmp/oc_engine_test.conf");
    int ok = b->eng_multipv == 3 && b->eng_threads == 4 && b->eng_hash == 128 &&
             b->eng_time_ms == 500 && b->eng_depth == 12 &&
             b->sound == false && b->max_fps == 144;
    gui_destroy(b);
    remove("/tmp/oc_engine_test.conf");
    if (!ok) {
        fprintf(stderr, "engine config round-trip failed\n");
        return 1;
    }
    return 0;
}

/* Window coordinate of a board square's centre, via the renderer transform. */
static void sq_window(Gui *g, SDL_Renderer *ren, int sq, int *wx, int *wy)
{
    int file = sq % 8, rank = sq / 8;
    int col = g->flipped ? 7 - file : file;
    int row = g->flipped ? rank : 7 - rank;
    float bx = g->board_x + col * g->sq + g->sq / 2.0f;
    float by = g->board_y + row * g->sq + g->sq / 2.0f;
    SDL_RenderLogicalToWindow(ren, bx, by, wx, wy);
}

/* Inverse of set_mouse using the transform the app installed. */
static void tf_window(const Gui *g, float bx, float by, int *wx, int *wy)
{
    float s  = g->tf_scale > 0.0f ? g->tf_scale : 1.0f;
    float ux = g->tf_ux > 0.0f ? g->tf_ux : 1.0f;
    float uy = g->tf_uy > 0.0f ? g->tf_uy : 1.0f;
    *wx = (int)lroundf((g->tf_vpx + bx) * s / ux);
    *wy = (int)lroundf((g->tf_vpy + by) * s / uy);
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("gui test", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
                                       SDL_WINDOW_HIDDEN);
    if (!win) { fprintf(stderr, "createwindow: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "createrenderer: %s\n", SDL_GetError()); return 1; }

    Gui *g = gui_create();
    if (!g) return 1;
    gui_init_assets(g, ren);
    if (!g->font_ui || !g->tex_piece[WP]) {
        fprintf(stderr, "font/piece textures missing\n");
        return 1;
    }
    if (g->boards.count < 30 || g->pieces.count < 30) {
        fprintf(stderr, "theme manifest not loaded (boards=%d pieces=%d)\n",
                g->boards.count, g->pieces.count);
        return 1;
    }

    if (test_engine_config() != 0) return 1;

    /* ---- welcome menu renders (title + mode list) ---- */
    g->scene = SCENE_MENU;
    gui_render(g, ren);
    if (count_non_bg(ren, 0, 100, WIN_W, 360, 0x161A22) < 300) {
        fprintf(stderr, "welcome menu appears blank\n");
        return 1;
    }

    /* Enter on the default (Analysis) entry transitions into the game. */
    SDL_Event men = {0};
    men.type = SDL_KEYDOWN;
    men.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &men);
    if (g->scene != SCENE_GAME || g->mode != MODE_ANALYSIS) {
        fprintf(stderr, "menu did not start analysis mode\n");
        return 1;
    }

    /* analysis engine: live evaluation arrives (when an engine is present) */
    if (g->engine_path[0]) {
        Uint32 et = SDL_GetTicks();
        int guard = 0;
        while (!g->eval_valid && guard++ < 600) {
            gui_tick(g, et);
            SDL_Delay(10);
            et += 10;
        }
        if (!g->eval_valid) {
            fprintf(stderr, "analysis evaluation did not arrive\n");
            return 1;
        }
    }

    /* play some moves into the gui board */
    const char *moves[] = { "e4", "e5", "Nf3", "Nc6", "Bc4", "Nf6", "O-O", NULL };
    bool ok = true;
    for (int i = 0; moves[i]; i++) {
        if (!san_apply(&g->board, moves[i])) {
            fprintf(stderr, "move %s failed\n", moves[i]);
            ok = false;
        }
    }
    if (!ok) return 1;

    g->selected = algebraic_to_sq("c4");
    g->targets.count = 0;

    gui_render(g, ren);

    int w, h;
    SDL_GetRendererOutputSize(ren, &w, &h);
    SDL_Surface *surf = SDL_CreateRGBSurface(0, w, h, 32,
                                             0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (!surf) { fprintf(stderr, "surface: %s\n", SDL_GetError()); return 1; }
    if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surf->pixels, surf->pitch) != 0) {
        fprintf(stderr, "readpixels: %s\n", SDL_GetError());
        return 1;
    }

    /* verify the board is actually drawn: the board area must contain many
     * pixels that are neither background nor pure board colors (pieces). */
    int drawn = 0;
    Uint32 bg = SDL_MapRGBA(surf->format, 30, 30, 34, 255);
    for (int y = BOARD_Y; y < BOARD_Y + 8 * SQ_SIZE; y += 2)
        for (int x = BOARD_X; x < BOARD_X + 8 * SQ_SIZE; x += 2) {
            Uint32 *px = (Uint32 *)((Uint8 *)surf->pixels + y * surf->pitch + x * 4);
            if (*px != bg) drawn++;
        }
    if (drawn < 400) {
        fprintf(stderr, "board area too empty (drawn pixels=%d)\n", drawn);
        return 1;
    }

    /* ---- flipped board still renders the pieces ---- */
    g->flipped = true;
    gui_render(g, ren);
    if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surf->pixels, surf->pitch) != 0) {
        fprintf(stderr, "readpixels (flipped): %s\n", SDL_GetError());
        return 1;
    }
    int fdrawn = 0;
    for (int y = BOARD_Y; y < BOARD_Y + 8 * SQ_SIZE; y += 2)
        for (int x = BOARD_X; x < BOARD_X + 8 * SQ_SIZE; x += 2) {
            Uint32 *px = (Uint32 *)((Uint8 *)surf->pixels + y * surf->pitch + x * 4);
            if (*px != bg) fdrawn++;
        }
    if (fdrawn < 400) {
        fprintf(stderr, "flipped board area too empty (drawn pixels=%d)\n", fdrawn);
        return 1;
    }
    /* corner mapping: the a1 light square should now sit at the top-right */
    g->flipped = false;

    /* white king texture should have visible (non-transparent) content */
    SDL_Surface *k = SDL_CreateRGBSurface(0, 1, 1, 32,
                                          0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (k) {
        SDL_RenderClear(ren);
        SDL_RenderSetClipRect(ren, &(SDL_Rect){0, 0, 1, 1});
        SDL_RenderCopy(ren, g->tex_piece[WK], NULL, &(SDL_Rect){0, 0, 1, 1});
        SDL_RenderReadPixels(ren, &(SDL_Rect){0, 0, 1, 1}, SDL_PIXELFORMAT_ARGB8888,
                             k->pixels, 4);
        Uint32 *c = k->pixels;
        if ((*c & 0xff000000) == 0) {
            fprintf(stderr, "white king glyph appears blank\n");
            return 1;
        }
        SDL_FreeSurface(k);
    }

    /* ---- event handling: on-demand SAN input ---- */
    do_restart_test(g);
    if (g->san_open) {
        fprintf(stderr, "SAN box should start closed\n");
        return 1;
    }

    SDL_Event ev = {0};
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &ev);
    if (!g->san_open) {
        fprintf(stderr, "Enter did not open the SAN box\n");
        return 1;
    }

    ev.type = SDL_TEXTINPUT;
    snprintf(ev.text.text, sizeof ev.text.text, "e4");
    gui_handle_event(g, &ev);
    ev.type = SDL_KEYDOWN;
    ev.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &ev);
    if (g->ply != 1 || g->board.side != BLACK) {
        fprintf(stderr, "text SAN input did not apply move\n");
        return 1;
    }
    if (g->san_open) {
        fprintf(stderr, "SAN box stayed open after submit\n");
        return 1;
    }
    if (strcmp(g->move_san[0], "e4") != 0) {
        fprintf(stderr, "move list SAN wrong: %s\n", g->move_san[0]);
        return 1;
    }

    /* ---- PGN export through the save prompt ---- */
    setenv("XDG_DATA_HOME", "/tmp/openchess-test-data", 1);
    snprintf(g->white_name, sizeof g->white_name, "Alice");
    snprintf(g->black_name, sizeof g->black_name, "Bob");
    SDL_Event save = {0};
    save.type = SDL_KEYDOWN;
    save.key.keysym.sym = SDLK_s;
    save.key.keysym.mod = KMOD_LCTRL;
    gui_handle_event(g, &save);
    if (!g->pgn_prompt) {
        fprintf(stderr, "Ctrl+S did not open the PGN prompt\n");
        return 1;
    }
    const char *nm = "smoke";
    for (const char *p = nm; *p; p++) {
        SDL_Event te = {0};
        te.type = SDL_TEXTINPUT;
        te.text.text[0] = *p;
        te.text.text[1] = '\0';
        gui_handle_event(g, &te);
    }
    SDL_Event ent = {0};
    ent.type = SDL_KEYDOWN;
    ent.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &ent);
    if (g->pgn_prompt) {
        fprintf(stderr, "PGN prompt did not close after save\n");
        return 1;
    }
    FILE *pf = fopen("/tmp/openchess-test-data/openchess/games/smoke.pgn", "r");
    if (!pf) {
        fprintf(stderr, "PGN file was not written\n");
        return 1;
    }
    char pgbuf[1024];
    size_t pgn = fread(pgbuf, 1, sizeof pgbuf - 1, pf);
    pgbuf[pgn] = '\0';
    fclose(pf);
    if (!strstr(pgbuf, "[White \"Alice\"]") || !strstr(pgbuf, "1. e4")) {
        fprintf(stderr, "PGN content unexpected:\n%s\n", pgbuf);
        return 1;
    }

    /* ---- event handling: mouse click move ---- */
    do_restart_test(g);
    int p1 = algebraic_to_sq("e2");           /* WHITE pawn after restart */
    SDL_Event md = {0}, mu = {0};
    md.type = SDL_MOUSEBUTTONDOWN; md.button.button = SDL_BUTTON_LEFT;
    md.button.x = BOARD_X + (p1 % 8) * SQ_SIZE + SQ_SIZE / 2;
    md.button.y = BOARD_Y + (7 - p1 / 8) * SQ_SIZE + SQ_SIZE / 2;
    gui_handle_event(g, &md);
    if (g->selected != p1) {
        fprintf(stderr, "click did not select pawn\n");
        return 1;
    }
    int t1 = algebraic_to_sq("e3");
    mu.type = SDL_MOUSEBUTTONUP; mu.button.button = SDL_BUTTON_LEFT;
    mu.button.x = BOARD_X + (t1 % 8) * SQ_SIZE + SQ_SIZE / 2;
    mu.button.y = BOARD_Y + (7 - t1 / 8) * SQ_SIZE + SQ_SIZE / 2;
    gui_handle_event(g, &mu);
    if (g->ply != 1 || g->board.side != BLACK) {
        fprintf(stderr, "click move did not apply (ply=%d side=%d)\n", g->ply, g->board.side);
        return 1;
    }

    /* ---- undo ---- */
    SDL_Event euk = {0};
    euk.type = SDL_KEYDOWN;
    euk.key.keysym.sym = SDLK_u;
    euk.key.keysym.mod = KMOD_LCTRL;
    gui_handle_event(g, &euk);
    if (g->ply != 0) {
        fprintf(stderr, "ctrl+u did not undo\n");
        return 1;
    }

    /* ---- restart ---- */
    SDL_Event erk = {0};
    erk.type = SDL_KEYDOWN;
    erk.key.keysym.sym = SDLK_r;
    erk.key.keysym.mod = KMOD_LCTRL;
    gui_handle_event(g, &erk);
    if (g->ply != 0 || g->board.side != WHITE) {
        fprintf(stderr, "ctrl+r did not restart\n");
        return 1;
    }

    /* ---- promotion chooser via click ---- */
    do_restart_test(g);
    memset(&g->board, 0, sizeof g->board);
    g->board.side = WHITE;
    g->board.castling = 0;
    g->board.board[algebraic_to_sq("g7")] = WP;
    g->board.board[algebraic_to_sq("f8")] = BR;
    g->board.board[algebraic_to_sq("h8")] = BK;
    g->board.board[algebraic_to_sq("g1")] = WK;
    g->state = game_state(&g->board);

    SDL_Event pd = {0}, pu = {0}, pc = {0};
    pd.type = SDL_MOUSEBUTTONDOWN; pd.button.button = SDL_BUTTON_LEFT;
    pd.button.x = BOARD_X + (algebraic_to_sq("g7") % 8) * SQ_SIZE + SQ_SIZE / 2;
    pd.button.y = BOARD_Y + (7 - algebraic_to_sq("g7") / 8) * SQ_SIZE + SQ_SIZE / 2;
    gui_handle_event(g, &pd);
    pu.type = SDL_MOUSEBUTTONUP; pu.button.button = SDL_BUTTON_LEFT;
    pu.button.x = BOARD_X + (algebraic_to_sq("f8") % 8) * SQ_SIZE + SQ_SIZE / 2;
    pu.button.y = BOARD_Y + (7 - algebraic_to_sq("f8") / 8) * SQ_SIZE + SQ_SIZE / 2;
    gui_handle_event(g, &pu);
    if (g->promo_from < 0) {
        fprintf(stderr, "promotion chooser did not open\n");
        return 1;
    }
    /* click the queen button (first chooser slot, positioned above f8) */
    SDL_Rect qr = { BOARD_X + (algebraic_to_sq("f8") % 8) * SQ_SIZE - 8,
                    BOARD_Y + (7 - algebraic_to_sq("f8") / 8) * SQ_SIZE + SQ_SIZE + 10,
                    50, 46 };
    pc.type = SDL_MOUSEBUTTONDOWN; pc.button.button = SDL_BUTTON_LEFT;
    pc.button.x = qr.x + 25;
    pc.button.y = qr.y + 23;
    gui_handle_event(g, &pc);
    if (g->board.board[algebraic_to_sq("f8")] != WQ) {
        fprintf(stderr, "promotion did not become queen\n");
        return 1;
    }
    if (g->ply != 1) {
        fprintf(stderr, "promotion move not recorded\n");
        return 1;
    }

    /* ---- animation queue drains with synthetic timestamps ---- */
    if (g->anim_count == 0) {
        fprintf(stderr, "move did not enqueue an animation\n");
        return 1;
    }
    Uint32 at = 1000;
    while (g->anim_count > 0 && at < 100000) {
        gui_anim_advance(g, at);
        at += 100;
    }
    if (g->anim_count != 0) {
        fprintf(stderr, "animation queue did not drain\n");
        return 1;
    }

    /* ---- FEN field: load a position, then reject a bad one ---- */
    do_restart_test(g);
    g->scene = SCENE_GAME;
    const char *kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    snprintf(g->fen_text, sizeof g->fen_text, "%s", kiwi);
    g->fen_len = (int)strlen(g->fen_text);
    g->fen_active = true;
    SDL_Event fev = {0};
    fev.type = SDL_KEYDOWN;
    fev.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &fev);
    if (g->fen_active) { fprintf(stderr, "FEN field stayed focused after load\n"); return 1; }
    if (g->board.side != WHITE || g->board.board[algebraic_to_sq("e5")] != WN) {
        fprintf(stderr, "FEN position not applied\n");
        return 1;
    }
    if (strcmp(g->fen_text, kiwi) != 0) {
        fprintf(stderr, "FEN field not refreshed: %s\n", g->fen_text);
        return 1;
    }
    Board before = g->board;
    snprintf(g->fen_text, sizeof g->fen_text, "garbage");
    g->fen_len = (int)strlen(g->fen_text);
    g->fen_active = true;
    gui_handle_event(g, &fev);
    if (memcmp(&g->board, &before, sizeof g->board) != 0) {
        fprintf(stderr, "invalid FEN mutated the board\n");
        return 1;
    }
    g->fen_active = false;

    /* ---- singleplayer setup scene, and an AI move when available ---- */
    g->scene = SCENE_MENU;
    g->menu_index = 0;          /* Singleplayer entry */
    SDL_Event enter = {0};
    enter.type = SDL_KEYDOWN;
    enter.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &enter);
    if (g->scene != SCENE_SINGLE_SETUP) {
        fprintf(stderr, "singleplayer did not open setup screen\n");
        return 1;
    }
    gui_render(g, ren);         /* setup screen renders */

    g->setup_side = 1;          /* play black so the AI (white) moves first */
    g->setup_level = 0;         /* Easy */
    gui_handle_event(g, &enter);

    if (g->engine_path[0]) {
        if (g->scene != SCENE_GAME || g->mode != MODE_SINGLE) {
            fprintf(stderr, "singleplayer did not start\n");
            return 1;
        }
        if (!g->flipped) {
            fprintf(stderr, "singleplayer did not auto-flip to the human side\n");
            return 1;
        }
        Uint32 tick = SDL_GetTicks();
        int guard = 0;
        while (g->ply == 0 && guard++ < 600) {
            gui_tick(g, tick);
            SDL_Delay(10);
            tick += 10;
        }
        if (g->ply != 1 || g->board.side != BLACK) {
            fprintf(stderr, "AI did not make the opening move (ply=%d)\n", g->ply);
            return 1;
        }
    } else {
        printf("(no engine found; singleplayer AI test skipped)\n");
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
    }

    /* ---- appearance picker: renders thumbnails and applies choices ---- */
    g->scene = SCENE_MENU;
    g->menu_index = 5;          /* the Appearance entry */
    SDL_Event ape = {0};
    ape.type = SDL_KEYDOWN;
    ape.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &ape);
    if (g->scene != SCENE_APPEARANCE) {
        fprintf(stderr, "appearance picker did not open\n");
        return 1;
    }
    gui_render(g, ren);         /* builds board thumbnails for the page */
    if (count_non_bg(ren, 150, 170, WIN_W - 300, 470, 0x161A22) < 300) {
        fprintf(stderr, "appearance picker appears blank\n");
        return 1;
    }
    if (!g->board_thumbs || !g->board_thumbs[g->board_index]) {
        fprintf(stderr, "board thumbnail was not generated\n");
        return 1;
    }

    SDL_Event right = {0};
    right.type = SDL_KEYDOWN;
    right.key.keysym.sym = SDLK_RIGHT;
    int board_before = g->board_index;
    gui_handle_event(g, &right);
    if (g->boards.count > 1 && g->board_index == board_before) {
        fprintf(stderr, "board selection did not advance\n");
        return 1;
    }

    SDL_Event tab2 = {0};
    tab2.type = SDL_KEYDOWN;
    tab2.key.keysym.sym = SDLK_2;
    gui_handle_event(g, &tab2);
    if (g->ape_tab != 1) {
        fprintf(stderr, "appearance tab switch failed\n");
        return 1;
    }
    int piece_before = g->piece_index;
    gui_handle_event(g, &right);
    if (g->pieces.count > 1 && g->piece_index == piece_before) {
        fprintf(stderr, "piece selection did not advance\n");
        return 1;
    }

    SDL_Event esc = {0};
    esc.type = SDL_KEYDOWN;
    esc.key.keysym.sym = SDLK_ESCAPE;
    gui_handle_event(g, &esc);
    if (g->scene != SCENE_MENU) {
        fprintf(stderr, "appearance back did not return to menu\n");
        return 1;
    }
    g->config_dirty = false;    /* don't write a config during tests */

    /* ---- right-mouse annotations ---- */
    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    {
        gui_render(g, ren);
        int e2 = algebraic_to_sq("e2");
        int e4 = algebraic_to_sq("e4");
        int wx, wy, wx2, wy2;
        sq_window(g, ren, e2, &wx, &wy);
        sq_window(g, ren, e4, &wx2, &wy2);

        SDL_Event cd = {0};
        cd.type = SDL_MOUSEBUTTONDOWN;
        cd.button.button = SDL_BUTTON_RIGHT;
        cd.button.x = wx; cd.button.y = wy;
        SDL_Event cu = cd;
        cu.type = SDL_MOUSEBUTTONUP;
        SDL_Event cm = {0};
        cm.type = SDL_MOUSEMOTION;
        cm.motion.x = wx2; cm.motion.y = wy2;
        cm.motion.state = SDL_BUTTON_RMASK;
        SDL_Event cu2 = {0};
        cu2.type = SDL_MOUSEBUTTONUP;
        cu2.button.button = SDL_BUTTON_RIGHT;
        cu2.button.x = wx2; cu2.button.y = wy2;

        /* right-click toggles a square highlight on... */
        gui_handle_event(g, &cd);
        gui_handle_event(g, &cu);
        if (g->ann_square_count != 1 || g->ann_squares[0].sq != e2) {
            fprintf(stderr, "right-click did not add a square\n");
            return 1;
        }
        /* ...and a second one erases it. */
        gui_handle_event(g, &cd);
        gui_handle_event(g, &cu);
        if (g->ann_square_count != 0) {
            fprintf(stderr, "second right-click did not erase the square\n");
            return 1;
        }

        /* right-drag draws an arrow... */
        gui_handle_event(g, &cd);
        gui_handle_event(g, &cm);
        gui_handle_event(g, &cu2);
        if (g->ann_arrow_count != 1 || g->ann_arrows[0].from != e2 ||
            g->ann_arrows[0].to != e4) {
            fprintf(stderr, "right-drag did not add an arrow\n");
            return 1;
        }
        /* ...and drawing the same arrow again erases it. */
        gui_handle_event(g, &cd);
        gui_handle_event(g, &cm);
        gui_handle_event(g, &cu2);
        if (g->ann_arrow_count != 0) {
            fprintf(stderr, "second right-drag did not erase the arrow\n");
            return 1;
        }

        /* put both back, then a left click on the board clears them */
        gui_handle_event(g, &cd);
        gui_handle_event(g, &cu);
        gui_handle_event(g, &cd);
        gui_handle_event(g, &cm);
        gui_handle_event(g, &cu2);
        SDL_Event ld = {0};
        ld.type = SDL_MOUSEBUTTONDOWN;
        ld.button.button = SDL_BUTTON_LEFT;
        ld.button.x = wx2; ld.button.y = wy2;
        gui_handle_event(g, &ld);
        if (g->ann_square_count != 0 || g->ann_arrow_count != 0) {
            fprintf(stderr, "left click did not clear annotations\n");
            return 1;
        }
        gui_render(g, ren);
    }

    /* ---- engine-arrow toggle (analysis panel checkbox) ---- */
    {
        bool before = g->engine_arrows;
        int ax = g->panel_x + 78 + 8;
        int ay = g->board_y + 52 + 8;
        int wx, wy;
        gui_render(g, ren);
        SDL_RenderLogicalToWindow(ren, (float)ax, (float)ay, &wx, &wy);
        SDL_Event td = {0};
        td.type = SDL_MOUSEBUTTONDOWN;
        td.button.button = SDL_BUTTON_LEFT;
        td.button.x = wx; td.button.y = wy;
        gui_handle_event(g, &td);
        if (g->engine_arrows == before) {
            fprintf(stderr, "engine arrows toggle did not flip\n");
            return 1;
        }
        g->engine_arrows = before;
        g->config_dirty = false;
    }

    /* ---- returning to the menu stashes the game; Continue resumes it ---- */
    {
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        board_reset(&g->board);
        g->state = game_state(&g->board);
        g->flipped = false;
        g->ply = 0;
        g->selected = -1;
        g->saved.valid = false;
        gui_render(g, ren);

        int e2 = algebraic_to_sq("e2"), e4 = algebraic_to_sq("e4");
        SDL_Event md = {0}, mu = {0};
        md.type = SDL_MOUSEBUTTONDOWN; md.button.button = SDL_BUTTON_LEFT;
        md.button.x = BOARD_X + (e2 % 8) * SQ_SIZE + SQ_SIZE / 2;
        md.button.y = BOARD_Y + (7 - e2 / 8) * SQ_SIZE + SQ_SIZE / 2;
        mu.type = SDL_MOUSEBUTTONUP; mu.button.button = SDL_BUTTON_LEFT;
        mu.button.x = BOARD_X + (e4 % 8) * SQ_SIZE + SQ_SIZE / 2;
        mu.button.y = BOARD_Y + (7 - e4 / 8) * SQ_SIZE + SQ_SIZE / 2;
        gui_handle_event(g, &md);
        gui_handle_event(g, &mu);
        if (g->ply != 1) {
            fprintf(stderr, "resume test setup move failed\n");
            return 1;
        }

        /* click the Menu button (panel_x+352..432, win_h-120..-80) */
        int mwx, mwy;
        SDL_RenderLogicalToWindow(ren, (float)(g->panel_x + 392),
                                  (float)(g->win_h - 100), &mwx, &mwy);
        SDL_Event mc = {0};
        mc.type = SDL_MOUSEBUTTONDOWN;
        mc.button.button = SDL_BUTTON_LEFT;
        mc.button.x = mwx; mc.button.y = mwy;
        gui_handle_event(g, &mc);
        if (g->scene != SCENE_MENU) {
            fprintf(stderr, "Menu button did not return to the menu\n");
            return 1;
        }
        if (!g->saved.valid) {
            fprintf(stderr, "game was not stashed on return to menu\n");
            return 1;
        }
        if (g->menu_index != 0) {
            fprintf(stderr, "Continue was not selected on the menu\n");
            return 1;
        }

        SDL_Event ent = {0};
        ent.type = SDL_KEYDOWN;
        ent.key.keysym.sym = SDLK_RETURN;
        gui_handle_event(g, &ent);
        if (g->scene != SCENE_GAME || g->mode != MODE_ANALYSIS) {
            fprintf(stderr, "Continue did not resume the game\n");
            return 1;
        }
        if (g->ply != 1 || strcmp(g->move_san[0], "e4") != 0) {
            fprintf(stderr, "resumed game state is wrong (ply=%d)\n", g->ply);
            return 1;
        }
        g->config_dirty = false;
    }

    /* ---- online lobby scenes render (private room + matchmaking) ---- */
    {
        g->scene = SCENE_ONLINE;
        g->online = NULL;           /* no live session; render the idle state */
        g->online_ui = 0;
        g->online_focus = 0;
        snprintf(g->online_code_in, sizeof g->online_code_in, "ABCDEF");
        gui_render(g, ren);
        g->online_ui = 1;
        gui_render(g, ren);
        g->online_ui = 0;

        /* typing edits the focused field only (focus 2 = room code) */
        g->online_focus = 2;
        SDL_Event te = {0};
        te.type = SDL_TEXTINPUT;
        te.text.text[0] = 'z'; te.text.text[1] = '\0';
        g->online_code_in[0] = '\0';
        gui_handle_event(g, &te);
        if (g->online_code_in[0] != 'Z') {
            fprintf(stderr, "online room code input not captured/uppercased\n");
            return 1;
        }
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
    }

    /* ---- input must invert the installed transform exactly ---- */
    {
        gui_render(g, ren);              /* install the transform */
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        board_reset(&g->board);
        g->state = game_state(&g->board);
        g->flipped = false;
        g->selected = -1;
        int e2 = algebraic_to_sq("e2");
        float bx = g->board_x + 4 * g->sq + g->sq / 2.0f;
        float by = g->board_y + 6 * g->sq + g->sq / 2.0f;
        int wx, wy;
        tf_window(g, bx, by, &wx, &wy);
        SDL_Event md = {0};
        md.type = SDL_MOUSEBUTTONDOWN;
        md.button.button = SDL_BUTTON_LEFT;
        md.button.x = wx; md.button.y = wy;
        gui_handle_event(g, &md);
        if (g->selected != e2) {
            fprintf(stderr, "transform click wrong (sel=%d want=%d vp=%.0f,%.0f "
                    "win=(%d,%d) base=(%d,%d)\n",
                    g->selected, e2, g->tf_vpx, g->tf_vpy, wx, wy,
                    g->mouse.x, g->mouse.y);
            return 1;
        }
        g->selected = -1;
        gui_render(g, ren);
    }

    /* ---- settings screen renders every tab ---- */
    {
        g->scene = SCENE_SETTINGS;
        g->settings_tab = 0;
        gui_render(g, ren);
        g->settings_tab = 1;    /* Gameplay */
        gui_render(g, ren);
        g->settings_tab = 2;    /* Audio & Video */
        gui_render(g, ren);
        g->settings_tab = 0;
        g->scene = SCENE_GAME;
    }

    /* ---- drag the board corner to resize it ---- */
    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    float zoom_before = g->zoom;
    int gx = g->board_x + 8 * g->sq - 9;
    int gy = g->board_y + 8 * g->sq - 9;

    SDL_Event rd = {0};
    rd.type = SDL_MOUSEBUTTONDOWN;
    rd.button.button = SDL_BUTTON_LEFT;
    rd.button.x = gx; rd.button.y = gy;
    gui_handle_event(g, &rd);
    if (!g->resizing_board) {
        fprintf(stderr, "board grip did not start a resize\n");
        return 1;
    }
    int anchor_x = g->resize_start_bx;
    SDL_Event rm = {0};
    rm.type = SDL_MOUSEMOTION;
    rm.motion.x = gx + 56; rm.motion.y = gy + 56;
    rm.motion.state = SDL_BUTTON_LMASK;
    gui_handle_event(g, &rm);
    if (g->zoom <= zoom_before) {
        fprintf(stderr, "UI did not magnify (zoom %.2f->%.2f)\n",
                zoom_before, g->zoom);
        return 1;
    }
    /* The grabbed grip point must track the cursor: diagonal drag picks the
     * x axis, so zoom advances by dx / (grabbed base x). */
    float want = zoom_before + 56.0f / (float)anchor_x;
    if (fabsf(g->zoom - want) > 0.002f) {
        fprintf(stderr, "grip drift: zoom %.4f want %.4f (anchor %d)\n",
                g->zoom, want, anchor_x);
        return 1;
    }
    /* The grabbed point must stay exactly under the cursor while dragging. */
    {
        int pw, ph;
        SDL_RenderLogicalToWindow(ren, (float)g->resize_start_bx,
                                  (float)g->resize_start_by, &pw, &ph);
        if (abs(pw - (gx + 56)) > 2 || abs(ph - (gy + 56)) > 2) {
            fprintf(stderr, "grip tracking off: grip=(%d,%d) cursor=(%d,%d)\n",
                    pw, ph, gx + 56, gy + 56);
            return 1;
        }
    }
    SDL_Event ru = {0};
    ru.type = SDL_MOUSEBUTTONUP;
    ru.button.button = SDL_BUTTON_LEFT;
    ru.button.x = gx + 56; ru.button.y = gy + 56;
    gui_handle_event(g, &ru);
    if (g->resizing_board) {
        fprintf(stderr, "board resize did not end on mouse up\n");
        return 1;
    }
    gui_render(g, ren);          /* resized layout renders */

    /* ---- vertical-only drag uses the y anchor ---- */
    {
        int gx2, gy2, wx2, wy2;
        gx2 = g->board_x + 8 * g->sq - 9;
        gy2 = g->board_y + 8 * g->sq - 9;
        SDL_RenderLogicalToWindow(ren, (float)gx2, (float)gy2, &wx2, &wy2);

        SDL_Event vd = {0};
        vd.type = SDL_MOUSEBUTTONDOWN;
        vd.button.button = SDL_BUTTON_LEFT;
        vd.button.x = wx2; vd.button.y = wy2;
        gui_handle_event(g, &vd);
        if (!g->resizing_board) {
            fprintf(stderr, "grip did not start a vertical resize\n");
            return 1;
        }
        float z0 = g->zoom;
        int ay = g->resize_start_by;
        SDL_Event vm = {0};
        vm.type = SDL_MOUSEMOTION;
        vm.motion.x = wx2; vm.motion.y = wy2 + 60;
        vm.motion.state = SDL_BUTTON_LMASK;
        gui_handle_event(g, &vm);
        float want2 = z0 + 60.0f / (float)ay;
        if (fabsf(g->zoom - want2) > 0.002f) {
            fprintf(stderr, "grip vertical drift: zoom %.4f want %.4f (anchor %d)\n",
                    g->zoom, want2, ay);
            return 1;
        }
        SDL_Event vu = {0};
        vu.type = SDL_MOUSEBUTTONUP;
        vu.button.button = SDL_BUTTON_LEFT;
        vu.button.x = wx2; vu.button.y = wy2 + 60;
        gui_handle_event(g, &vu);
        gui_render(g, ren);
    }
    g->config_dirty = false;

    /* ---- clicking a square while magnified still selects that square ---- */
    {
        board_reset(&g->board);
        g->state = game_state(&g->board);
        g->flipped = false;
        g->selected = -1;

        int e2 = algebraic_to_sq("e2");
        int file = e2 % 8, rank = e2 / 8;
        int col = g->flipped ? 7 - file : file;
        int row = g->flipped ? rank : 7 - rank;
        int bx = g->board_x + col * g->sq + g->sq / 2;
        int by = g->board_y + row * g->sq + g->sq / 2;
        SDL_Event md = {0};
        md.type = SDL_MOUSEBUTTONDOWN;
        md.button.button = SDL_BUTTON_LEFT;
        md.button.x = (int)(bx * g->zoom + 0.5f);
        md.button.y = (int)(by * g->zoom + 0.5f);
        g->selected = -1;
        gui_handle_event(g, &md);
        if (g->selected != e2) {
            fprintf(stderr, "magnified click mapping wrong (selected=%d want=%d)\n",
                    g->selected, e2);
            return 1;
        }
        g->selected = -1;
    }

    /* ---- large (fullscreen-like) window: canvas centred, mapping exact ---- */
    g->scene = SCENE_MENU;
    SDL_SetWindowSize(g->win, 1600, 800);
    SDL_Event wr = {0};
    wr.type = SDL_WINDOWEVENT;
    wr.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
    wr.window.data1 = 1600;
    wr.window.data2 = 800;
    gui_handle_event(g, &wr);
    gui_render(g, ren);          /* re-applies the centred view */

    int wx = 0, wy = 0;
    SDL_RenderLogicalToWindow(ren, 590.0f, 400.0f, &wx, &wy);
    SDL_Event mm = {0};
    mm.type = SDL_MOUSEMOTION;
    mm.motion.x = wx;
    mm.motion.y = wy;
    gui_handle_event(g, &mm);
    if (abs(g->mouse.x - 590) > 1 || abs(g->mouse.y - 400) > 1) {
        fprintf(stderr, "large-window mapping wrong: base=(%d,%d) want=(590,400)\n",
                g->mouse.x, g->mouse.y);
        return 1;
    }

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    SDL_RenderLogicalToWindow(ren, 0.0f, 0.0f, &x0, &y0);
    SDL_RenderLogicalToWindow(ren, (float)g->win_w, (float)g->win_h, &x1, &y1);
    int winw = 0, winh = 0;
    SDL_GetWindowSize(g->win, &winw, &winh);
    if (abs(x0 - (winw - x1)) > 2 || abs(y0 - (winh - y1)) > 2) {
        fprintf(stderr, "canvas not centred: x0=%d x1=%d winw=%d y0=%d y1=%d winh=%d\n",
                x0, x1, winw, y0, y1, winh);
        return 1;
    }

    if (SDL_SaveBMP(surf, "gui_smoke.bmp") != 0) {
        fprintf(stderr, "savebmp: %s\n", SDL_GetError());
        return 1;
    }

    printf("GUI render + interaction smoke test passed\n");
    SDL_FreeSurface(surf);
    gui_destroy(g);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}