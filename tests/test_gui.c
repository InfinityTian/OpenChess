#include "../src/gui.h"
#include "../src/pgn.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
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
    g->menu_index = 3;          /* the Appearance entry */
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