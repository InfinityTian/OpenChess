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

static int count_rgb(SDL_Renderer *ren, SDL_Rect rect, Uint8 r, Uint8 g, Uint8 b)
{
    Uint8 *pixels = malloc((size_t)rect.w * rect.h * 4);
    if (!pixels) return -1;
    if (SDL_RenderReadPixels(ren, &rect, SDL_PIXELFORMAT_RGBA32,
                             pixels, rect.w * 4) != 0) { free(pixels); return -1; }
    int count = 0;
    for (int i = 0; i < rect.w * rect.h; i++)
        if (pixels[4 * i] == r && pixels[4 * i + 1] == g && pixels[4 * i + 2] == b)
            count++;
    free(pixels);
    return count;
}

static uint64_t region_hash(SDL_Renderer *ren, SDL_Rect rect)
{
    size_t bytes = (size_t)rect.w * rect.h * 4;
    Uint8 *pixels = malloc(bytes);
    if (!pixels) return 0;
    if (SDL_RenderReadPixels(ren, &rect, SDL_PIXELFORMAT_RGBA32,
                             pixels, rect.w * 4) != 0) { free(pixels); return 0; }
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < bytes; i++)
        hash = (hash ^ pixels[i]) * UINT64_C(1099511628211);
    free(pixels);
    return hash;
}

static float black_piece_x(SDL_Renderer *ren, SDL_Rect rect)
{
    size_t bytes = (size_t)rect.w * rect.h * 4;
    Uint8 *pixels = malloc(bytes);
    if (!pixels) return -1;
    if (SDL_RenderReadPixels(ren, &rect, SDL_PIXELFORMAT_RGBA32,
                             pixels, rect.w * 4) != 0) { free(pixels); return -1; }
    int count = 0;
    double sum = 0;
    for (int y = 0; y < rect.h; y++)
        for (int x = 0; x < rect.w; x++) {
            Uint8 *p = pixels + 4 * (y * rect.w + x);
            if (p[0] < 8 && p[1] < 8 && p[2] < 8) { sum += rect.x + x; count++; }
        }
    free(pixels);
    return count ? (float)(sum / count) : -1;
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

static void setup_promo(Gui *g)
{
    memset(&g->board, 0, sizeof g->board);
    g->board.side = WHITE;
    g->board.castling = 0;
    g->board.board[algebraic_to_sq("g7")] = WP;
    g->board.board[algebraic_to_sq("f8")] = BR;
    g->board.board[algebraic_to_sq("h8")] = BK;
    g->board.board[algebraic_to_sq("g1")] = WK;
    g->state = game_state(&g->board);
    g->ply = 0;
    g->selected = -1;
    g->promo_from = g->promo_to = -1;
    g->tree_cur = g->pgntree;
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

/* Navigation must track every input even when none of the preceding visual
 * transitions have finished, including a direction change and castling. */
static int test_analysis_navigation(SDL_Renderer *ren)
{
    Gui *g = gui_create();
    if (!g) return 1;
    g->eng_multipv = 0;
    gui_init_assets(g, ren);
    for (int c = RC_BOOK; c <= RC_MISS; c++) {
        if (!g->badge_tex[c]) {
            fprintf(stderr, "quality badge %d did not load\n", c);
            return 1;
        }
    }
    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    g->pgntree = mt_new_root(&g->board);
    g->tree_cur = g->pgntree;
    MoveNode *nodes[8] = { g->pgntree };
    const char *uci[] = { "e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6", "e1g1" };
    for (int i = 0; i < 7; i++) {
        Move m;
        if (!ai_uci_to_move(&nodes[i]->board, uci[i], &m)) return 1;
        nodes[i + 1] = mt_add_child(nodes[i], m, NULL);
        nodes[i + 1]->cls = RC_BRILLIANT + i;
    }
    gui_render(g, ren);
    SDL_Rect first_row={g->panel_x+420,g->board_y+226,4,10},second_row=first_row;second_row.y+=30;
    if(count_rgb(ren,first_row,226,237,248)<3||count_rgb(ren,second_row,226,237,248)>0){
        fprintf(stderr,"move-list alternating row contrast was lost\n");return 1;
    }
    SDL_Rect san_area = { g->panel_x + 68, g->board_y + 226, 60, 21 };
    if (count_rgb(ren, san_area, 27, 134, 112) < 3) {
        fprintf(stderr, "SAN text did not use its light-theme Brilliant grade colour\n");
        return 1;
    }
    g->ui->dark_mode=true;gui_render(g,ren);
    if(count_rgb(ren,san_area,39,194,163)<3){fprintf(stderr,"SAN grade colour lost in dark mode\n");return 1;}
    g->ui->dark_mode=false;gui_render(g,ren);
    SDL_Event click = {0};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    tf_window(g, g->panel_x + PANEL_W * 3 / 4, g->board_y + 398,
              &click.button.x, &click.button.y);
    for (int i = 1; i <= 7; i++) {
        gui_handle_event(g, &click);
        if (g->ply != i || g->tree_cur != nodes[i] || g->anim_count != 1 ||
            g->anim_queue[0].to != MOVE_TO(nodes[i]->move) ||
            memcmp(&g->board, &nodes[i]->board, sizeof(Board)) != 0) {
            fprintf(stderr, "rapid Next click left stale animation/position at ply %d\n", i);
            return 1;
        }
        /* The first two clicks arrive before any render; later ones arrive
         * halfway through the transition. */
        if (i >= 2) {
            gui_anim_advance(g, 1000);
            gui_anim_advance(g, 1000 + g->anim_queue[0].dur / 2);
        }
    }
    gui_handle_event(g, &click); /* End of line settles the final animation. */
    if (g->anim_count || g->ply != 7) return 1;

    SDL_Event key = {0};
    key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_LEFT;
    gui_handle_event(g, &key);
    if (g->anim_count != 1 || !g->anim_queue[0].has_rook ||
        g->anim_queue[0].rfrom != algebraic_to_sq("f1") ||
        g->anim_queue[0].rto != algebraic_to_sq("h1")) {
        fprintf(stderr, "reverse castling did not animate both pieces\n");
        return 1;
    }
    key.key.keysym.sym = SDLK_RIGHT;
    gui_handle_event(g, &key);
    if (g->anim_count != 1 || g->anim_queue[0].reverse || g->tree_cur != nodes[7]) {
        fprintf(stderr, "direction change retained a reverse transition\n");
        return 1;
    }

    gui_render(g, ren);
    bool jumped = false;
    for (int i = 0; i < g->move_hit_count; i++) {
        if (g->move_hit_node[i] != nodes[2]) continue;
        SDL_Rect r = g->move_hit_rect[i];
        tf_window(g, r.x + r.w / 2, r.y + r.h / 2,
                  &click.button.x, &click.button.y);
        gui_handle_event(g, &click);
        jumped = true;
        break;
    }
    if (!jumped || g->anim_count || g->tree_cur != nodes[2] || g->ply != 2) {
        fprintf(stderr, "move-list jump retained a stale animation\n");
        return 1;
    }
    g->anim_style = ANIM_NONE;
    gui_handle_event(g, &key);
    if (g->anim_count || g->tree_cur != nodes[3]) return 1;

    /* Switching sidebar tabs must not activate hidden move or FEN controls. */
    tf_window(g, g->panel_x + PANEL_W * 3 / 4, g->board_y + 205,
              &click.button.x, &click.button.y);
    gui_handle_event(g, &click);
    gui_render(g, ren);
    if (!g->analysis_summary || g->move_hit_count || g->fen_active) return 1;
    tf_window(g, g->panel_x + 40, g->board_y + 490,
              &click.button.x, &click.button.y);
    gui_handle_event(g, &click);
    if (g->fen_active || g->san_open || g->tree_cur != nodes[3]) return 1;
    tf_window(g, g->panel_x + PANEL_W / 4, g->board_y + 205,
              &click.button.x, &click.button.y);
    gui_handle_event(g, &click);
    gui_render(g, ren);
    if (g->analysis_summary || !g->move_hit_count) return 1;
    tf_window(g, g->panel_x + (PANEL_W - 1) * 2 / 7, g->board_y + 430,
              &click.button.x, &click.button.y);
    gui_handle_event(g, &click);
    if (g->tree_cur != nodes[2] || g->anim_count) {
        fprintf(stderr, "evaluation graph did not navigate to its move\n");
        return 1;
    }
    if (g->engine_path[0]) {
        key.key.keysym.sym = SDLK_v;
        gui_handle_event(g, &key);
        for (int i = 0; g->review_on && i < 400; i++) {
            gui_tick(g, SDL_GetTicks());
            SDL_Delay(10);
        }
        if (!g->rev_report || !nodes[0]->has_eval || !nodes[1]->has_eval ||
            !nodes[2]->has_eval || nodes[2]->eval_cp != -g->ra_cp ||
            nodes[1]->eval_cp != g->rb_cp) {
            fprintf(stderr, "review did not retain side-to-move evaluations for the graph\n");
            return 1;
        }
    }
    gui_destroy(g);
    return 0;
}

static void test_load_position(Gui *g, const char *fen)
{
    snprintf(g->fen_text, sizeof g->fen_text, "%s", fen);
    g->fen_len = (int)strlen(g->fen_text);
    g->fen_active = true;
    SDL_Event key = {0};
    key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &key);
}

static void test_play_san(Gui *g, const char *san)
{
    SDL_Event key = {0};
    key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_RETURN;
    gui_handle_event(g, &key);
    SDL_Event text = {0};
    text.type = SDL_TEXTINPUT;
    snprintf(text.text.text, sizeof text.text.text, "%s", san);
    gui_handle_event(g, &text);
    gui_handle_event(g, &key);
}

static int test_position_effects(SDL_Renderer *ren)
{
    Gui *g = gui_create();
    if (!g) return 1;
    g->eng_multipv = 0;
    gui_init_assets(g, ren);
    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    for (int i = 0; i < 3; i++) if (!g->end_tex[i]) return 1;

    test_load_position(g, "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    test_play_san(g, "Qg7#");
    if (g->state != CHECKMATE || !g->end_anim || g->check_anim) {
        fprintf(stderr, "mate did not start its own end effect\n");
        return 1;
    }
    g->anim_count = 0;
    g->end_anim = SDL_GetTicks() - 400;
    Board mate = g->board;
    gui_render(g, ren);
    if (memcmp(&mate, &g->board, sizeof mate) != 0) return 1;
    SDL_Event key = {0};
    key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_LEFT;
    gui_handle_event(g, &key);
    if (g->end_anim || g->state != NO_GAME_OVER) return 1;
    key.key.keysym.sym = SDLK_RIGHT;
    gui_handle_event(g, &key);
    if (!g->end_anim || g->state != CHECKMATE) return 1;
    g->anim_style = ANIM_NONE;
    key.key.keysym.sym = SDLK_LEFT;
    gui_handle_event(g, &key);
    key.key.keysym.sym = SDLK_RIGHT;
    gui_handle_event(g, &key);
    if (g->end_anim || g->anim_count || g->state != CHECKMATE) return 1;
    gui_render(g, ren);
    g->anim_style = ANIM_ARCADE;

    test_load_position(g, "7k/5Q2/6K1/8/8/8/8/8 w - - 0 1");
    test_play_san(g, "Qe6");
    if (g->state != STALEMATE || !g->end_anim || g->check_anim) {
        fprintf(stderr, "stalemate did not start the draw effect\n");
        return 1;
    }
    g->anim_count = 0;
    g->end_anim = SDL_GetTicks() - 400;
    Board draw = g->board;
    gui_render(g, ren);
    if (memcmp(&draw, &g->board, sizeof draw) != 0) return 1;

    test_load_position(g, "7k/8/6K1/8/8/8/8/8 w - - 0 1");
    if (g->state != INSUFFICIENT_MATERIAL || g->end_anim) return 1;
    gui_render(g, ren); /* direct FEN jumps show settled draw badges */

    test_load_position(g, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    gui_render(g, ren);
    SDL_Event click = {0};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    sq_window(g, ren, algebraic_to_sq("e2"), &click.button.x, &click.button.y);
    click.button.x += 30;
    click.button.y -= 22;
    gui_handle_event(g, &click);
    SDL_Rect piece_area = { g->board_x + 4 * g->sq - 16,
                            g->board_y + 5 * g->sq + 24, g->sq + 26, 130 };
    SDL_Rect ripple = { g->board_x + 3 * g->sq, g->board_y + 5 * g->sq,
                        2 * g->sq, 2 * g->sq };
    g->pick_started = SDL_GetTicks() - 100;
    gui_render(g, ren);
    float cursor_x = black_piece_x(ren, piece_area);
    if (fabsf(cursor_x - g->mouse.x) > 2) {
        fprintf(stderr, "off-centre pickup did not place the piece at the cursor: measured=%.1f pointer=%d selected=%d scale=%.2f area=%d,%d,%d,%d\n",cursor_x,g->mouse.x,g->selected,g->tf_scale,piece_area.x,piece_area.y,piece_area.w,piece_area.h);
        return 1;
    }
    uint64_t pressed = region_hash(ren, ripple);
    click.type = SDL_MOUSEBUTTONUP;
    gui_handle_event(g, &click);
    if (!g->pick_released || g->selected != algebraic_to_sq("e2")) return 1;
    g->pick_released = SDL_GetTicks() - 80;
    g->pick_started = g->pick_released - 100;
    gui_render(g, ren);
    uint64_t fading = region_hash(ren, ripple);
    float returning_x = black_piece_x(ren, piece_area);
    g->pick_released = SDL_GetTicks() - 400;
    gui_render(g, ren);
    uint64_t settled = region_hash(ren, ripple);
    float centre_x = black_piece_x(ren, piece_area);
    float expected_x = g->board_x + 4.5f * g->sq;
    if (fabsf(centre_x - expected_x) > 2 ||
        returning_x <= centre_x + 2 || returning_x >= cursor_x - 2) {
        fprintf(stderr, "picked piece did not smoothly return to its square centre\n");
        return 1;
    }
    if (!pressed || pressed == fading || fading == settled) {
        fprintf(stderr, "click bloom did not decay after release\n");
        return 1;
    }
    g->pick_released = SDL_GetTicks() - 1300;
    gui_render(g, ren);
    if (settled != region_hash(ren, ripple)) {
        fprintf(stderr, "released pickup restarted its bloom\n");
        return 1;
    }
    click.type = SDL_MOUSEBUTTONDOWN;
    gui_handle_event(g, &click);
    if (g->selected >=0 || g->targets.count) {
        fprintf(stderr, "clicking the same piece did not cancel selection\n");
        return 1;
    }
    gui_handle_event(g, &click); /* Select again for animation checks. */
    g->anim_style = ANIM_NONE;
    g->pick_started = SDL_GetTicks() - 340;
    gui_render(g, ren);
    uint64_t disabled = region_hash(ren, ripple);
    g->pick_started = SDL_GetTicks() - 700;
    gui_render(g, ren);
    if (disabled != region_hash(ren, ripple)) return 1;
    g->anim_style = ANIM_ARCADE;
    SDL_Event motion = {0};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.state = SDL_BUTTON_LMASK;
    motion.motion.x = click.button.x + 1; motion.motion.y = click.button.y;
    gui_handle_event(g, &motion);
    if (g->dragging) { fprintf(stderr, "click jitter started a drag\n"); return 1; }
    sq_window(g, ren, algebraic_to_sq("e4"), &motion.motion.x, &motion.motion.y);
    motion.motion.x += 12;
    gui_handle_event(g, &motion);
    if (!g->dragging) return 1;
    SDL_Rect held = { g->mouse.x - g->sq, g->mouse.y - g->sq,
                      2 * g->sq, 2 * g->sq };
    /* No pointer events between these frames: the pulse must keep running. */
    g->pick_started = SDL_GetTicks() - 450;
    gui_render(g, ren);
    int large = count_rgb(ren, held, 0, 0, 0);
    g->pick_started = SDL_GetTicks() - 900;
    gui_render(g, ren);
    int normal = count_rgb(ren, held, 0, 0, 0);
    if (normal <= 0 || large < normal * 11 / 10) {
        fprintf(stderr, "held piece did not pulse in size (%d vs %d pixels)\n", large, normal);
        return 1;
    }
    g->pick_started = SDL_GetTicks() - 1350; /* pulse again on the next cycle */
    gui_render(g, ren);
    if (count_rgb(ren, held, 0, 0, 0) < normal * 11 / 10) return 1;
    click.type = SDL_MOUSEBUTTONUP;
    click.button.x = motion.motion.x; click.button.y = motion.motion.y;
    gui_handle_event(g, &click);
    if (g->ply != 1 || g->anim_count != 1 || !g->anim_queue[0].fast ||
        fabsf(g->anim_queue[0].cursor_x - g->mouse.x) > 1 ||
        g->anim_queue[0].cursor_scale < 1.10f) {
        fprintf(stderr, "drop did not settle from the pointer position\n");
        return 1;
    }
    gui_destroy(g);
    return 0;
}

int main(void)
{
    /* Paths are cached on first use (including puzzle loading in gui_create). */
    setenv("XDG_DATA_HOME", "/tmp/openchess-test-data", 1);
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

    /* Never touch the user's real config: redirect to a temp file. */
    setenv("OPENCHESS_CONFIG", "/tmp/oc_test_gui.conf", 1);
    remove("/tmp/oc_test_gui.conf");

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
    /* factory defaults: Icy Sea board + Cases pieces + Arcade animation */
    if (strcmp(g->boards.items[g->board_index].key, "icy_sea") != 0 ||
        strcmp(g->pieces.items[g->piece_index].key, "cases") != 0 ||
        g->anim_style != ANIM_ARCADE) {
        fprintf(stderr, "unexpected appearance defaults: %s/%s/%d\n",
                g->boards.items[g->board_index].key,
                g->pieces.items[g->piece_index].key, (int)g->anim_style);
        return 1;
    }

    if (test_engine_config() != 0) return 1;
    if (test_analysis_navigation(ren) != 0) return 1;
    if (test_position_effects(ren) != 0) return 1;

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
    SDL_Rect qr = { BOARD_X + (algebraic_to_sq("f8") % 8) * SQ_SIZE,
                    BOARD_Y, SQ_SIZE, SQ_SIZE };
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

    /* ---- underpromotion to knight via the chooser ---- */
    setup_promo(g);
    gui_handle_event(g, &pd);   /* select g7 */
    gui_handle_event(g, &pu);   /* release on f8 -> chooser */
    if (g->promo_from < 0) {
        fprintf(stderr, "underpromotion chooser did not open\n");
        return 1;
    }
    {
        SDL_Rect nr = qr;
        nr.y += SQ_SIZE;         /* slot 1 = knight */
        SDL_Event kc = pc;
        kc.button.x = nr.x + 25;
        kc.button.y = nr.y + 23;
        gui_handle_event(g, &kc);
    }
    if (g->board.board[algebraic_to_sq("f8")] != WN) {
        fprintf(stderr, "underpromotion to knight failed (got %d)\n",
                g->board.board[algebraic_to_sq("f8")]);
        return 1;
    }

    /* ---- double click the target defaults to the queen ---- */
    setup_promo(g);
    gui_handle_event(g, &pd);
    gui_handle_event(g, &pu);
    if (g->promo_from < 0) {
        fprintf(stderr, "double-click chooser did not open\n");
        return 1;
    }
    {
        SDL_Event dc = pc;
        dc.button.x = BOARD_X + (algebraic_to_sq("f8") % 8) * SQ_SIZE + SQ_SIZE / 2;
        dc.button.y = BOARD_Y + (7 - algebraic_to_sq("f8") / 8) * SQ_SIZE + SQ_SIZE / 2;
        gui_handle_event(g, &dc);
    }
    if (g->board.board[algebraic_to_sq("f8")] != WQ) {
        fprintf(stderr, "double-click did not promote to queen\n");
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
    g->menu_index = 6;          /* the Appearance entry */
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
        int ax = g->panel_x + 4 + 12;
        int ay = g->board_y + 82 + 8;
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

    /* ---- puzzle setup renders and a puzzle can be solved ---- */
    {
        g->scene = SCENE_PUZZLE_SETUP;
        g->setup_field = 0;
        gui_render(g, ren);
        g->setup_field = 1;
        gui_render(g, ren);

        const char *ppath = "/tmp/oc_gui_puzzle.jsonl";
        FILE *pf = fopen(ppath, "w");
        if (pf) {
            fputs("{\"id\":\"t1\",\"fen\":\"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/"
                  "RNBQKBNR w KQkq - 0 1\",\"moves\":\"e2e4 e7e5\","
                  "\"rating\":1500,\"themes\":[\"opening\"]}\n", pf);
            fclose(pf);
            g->puzzles = puzzles_load(ppath);
        }
        if (g->puzzles) {
            g->puzzle_band = 2;      /* Normal: around my rating (1500). */
            g->setup_field = 0;      /* Enter on the theme field now opens its dropdown. */
            g->puzzle_theme = 0;
            SDL_Event st = {0};
            st.type = SDL_KEYDOWN;
            st.key.keysym.sym = SDLK_RETURN;
            gui_handle_event(g, &st);
            if (g->scene != SCENE_GAME || g->mode != MODE_PUZZLE) {
                fprintf(stderr, "puzzle did not start\n");
                return 1;
            }
            /* a wrong move (e7-e6) scores as incorrect and drops the rating */
            gui_render(g, ren);
            int e7 = algebraic_to_sq("e7"), e6 = algebraic_to_sq("e6");
            int cwx, cwy, cwx2, cwy2;
            sq_window(g, ren, e7, &cwx, &cwy);
            sq_window(g, ren, e6, &cwx2, &cwy2);
            SDL_Event wd = {0};
            wd.type = SDL_MOUSEBUTTONDOWN; wd.button.button = SDL_BUTTON_LEFT;
            wd.button.x = cwx; wd.button.y = cwy;
            gui_handle_event(g, &wd);
            SDL_Event wu = {0};
            wu.type = SDL_MOUSEBUTTONUP; wu.button.button = SDL_BUTTON_LEFT;
            wu.button.x = cwx2; wu.button.y = cwy2;
            gui_handle_event(g, &wu);
            if (!g->puzzle_counted || g->puzzle_rating >= 1500) {
                fprintf(stderr, "wrong puzzle move did not lower the rating\n");
                return 1;
            }
            /* solver (Black) plays e5 via the SAN box */
            SDL_Event en = {0};
            en.type = SDL_KEYDOWN;
            en.key.keysym.sym = SDLK_RETURN;
            gui_handle_event(g, &en);        /* open box */
            SDL_Event te = {0};
            te.type = SDL_TEXTINPUT;
            te.text.text[0] = 'e'; te.text.text[1] = '5';
            te.text.text[2] = '\0';
            gui_handle_event(g, &te);
            gui_handle_event(g, &en);        /* submit */
            if (!g->puzzle_done) {
                fprintf(stderr, "puzzle solution not accepted\n");
                return 1;
            }
            puzzles_free(g->puzzles);
            g->puzzles = NULL;
            remove(ppath);
        } else {
            printf("(puzzle test skipped: could not load sample)\n");
        }
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
    }

    /* ---- PGN import pop-up loads the mainline into the board ---- */
    {
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        SDL_Event o = {0};
        o.type = SDL_KEYDOWN;
        o.key.keysym.sym = SDLK_o;
        o.key.keysym.mod = KMOD_LCTRL;
        gui_handle_event(g, &o);
        if (!g->pgn_import_open) {
            fprintf(stderr, "Ctrl+O did not open the PGN import\n");
            return 1;
        }
        snprintf(g->pgn_text, sizeof g->pgn_text, "1. e4 e5 2. Nf3 Nc6");
        g->pgn_text_len = (int)strlen(g->pgn_text);
        SDL_Event en = {0};
        en.type = SDL_KEYDOWN;
        en.key.keysym.sym = SDLK_RETURN;
        gui_handle_event(g, &en);
        if (g->ply != 4 || g->pgntree == NULL) {
            fprintf(stderr, "PGN import did not load the mainline (ply=%d)\n", g->ply);
            return 1;
        }
        if (strcmp(g->move_san[0], "e4") != 0 || strcmp(g->move_san[3], "Nc6") != 0) {
            fprintf(stderr, "PGN import move list wrong\n");
            return 1;
        }
        if (g->pgn_import_open) {
            fprintf(stderr, "import pop-up stayed open\n");
            return 1;
        }

        /* click the first move (e4) to jump back, then play c5 -> variation */
        gui_render(g, ren);
        int idx = -1;
        for (int i = 0; i < g->move_hit_count; i++)
            if (strcmp(g->move_hit_node[i]->san, "e4") == 0) { idx = i; break; }
        if (idx < 0) {
            fprintf(stderr, "no clickable move hit for e4\n");
            return 1;
        }
        MoveNode *e4node = g->move_hit_node[idx];
        int bx = g->move_hit_rect[idx].x + 20;
        int by = g->move_hit_rect[idx].y + 8;
        int wx, wy;
        SDL_RenderLogicalToWindow(ren, (float)bx, (float)by, &wx, &wy);
        SDL_Event md = {0};
        md.type = SDL_MOUSEBUTTONDOWN;
        md.button.button = SDL_BUTTON_LEFT;
        md.button.x = wx; md.button.y = wy;
        gui_handle_event(g, &md);
        if (g->tree_cur != e4node || g->ply != 1) {
            fprintf(stderr, "clicking a move did not navigate (ply=%d)\n", g->ply);
            return 1;
        }
        SDL_Event en3 = {0};
        en3.type = SDL_KEYDOWN;
        en3.key.keysym.sym = SDLK_RETURN;
        gui_handle_event(g, &en3);           /* open SAN box */
        SDL_Event tc = {0};
        tc.type = SDL_TEXTINPUT;
        tc.text.text[0] = 'c'; tc.text.text[1] = '5'; tc.text.text[2] = '\0';
        gui_handle_event(g, &tc);
        gui_handle_event(g, &en3);           /* submit */
        if (!g->tree_cur || strcmp(g->tree_cur->san, "c5") != 0 ||
            !g->tree_cur->parent || !g->tree_cur->parent->first ||
            strcmp(g->tree_cur->parent->first->san, "e5") != 0) {
            fprintf(stderr, "playing after navigating did not create a variation\n");
            return 1;
        }

        /* the variation move is rendered and clickable in the tree panel */
        gui_render(g, ren);
        int saw_c5 = 0, saw_e4 = 0;
        for (int i = 0; i < g->move_hit_count; i++) {
            if (strcmp(g->move_hit_node[i]->san, "c5") == 0) saw_c5 = 1;
            if (strcmp(g->move_hit_node[i]->san, "e4") == 0) saw_e4 = 1;
        }
        if (!saw_c5 || !saw_e4) {
            fprintf(stderr, "move tree missing variation tokens (c5=%d e4=%d)\n",
                    saw_c5, saw_e4);
            return 1;
        }

        /* a long game overflows the box and the wheel scrolls it */
        {
            SDL_Event o2 = {0};
            o2.type = SDL_KEYDOWN;
            o2.key.keysym.sym = SDLK_o;
            o2.key.keysym.mod = KMOD_LCTRL;
            gui_handle_event(g, &o2);
            const char *longpgn =
                "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 "
                "6. Re1 b5 7. Bb3 d6 8. c3 O-O 9. h3 Nb8 10. d4 Nbd7 "
                "11. c4 c6 12. cxb5 axb5 13. Nc3 Bb7 14. Bg5 b4 15. Nb1 h6";
            snprintf(g->pgn_text, sizeof g->pgn_text, "%s", longpgn);
            g->pgn_text_len = (int)strlen(g->pgn_text);
            SDL_Event lenter = {0};
            lenter.type = SDL_KEYDOWN;
            lenter.key.keysym.sym = SDLK_RETURN;
            gui_handle_event(g, &lenter);
            gui_render(g, ren);
            if (g->move_scroll_max <= 0) {
                fprintf(stderr, "long move tree did not overflow (%d)\n",
                        g->move_scroll_max);
                return 1;
            }
            int wx, wy;
            SDL_RenderLogicalToWindow(ren,
                (float)(g->move_box.x + g->move_box.w / 2),
                (float)(g->move_box.y + g->move_box.h / 2), &wx, &wy);
            SDL_Event mm = {0};
            mm.type = SDL_MOUSEMOTION;
            mm.motion.x = wx; mm.motion.y = wy;
            gui_handle_event(g, &mm);
            SDL_Event wh = {0};
            wh.type = SDL_MOUSEWHEEL;
            wh.wheel.y = -1;
            gui_handle_event(g, &wh);
            if (g->move_scroll <= 0) {
                fprintf(stderr, "mouse wheel did not scroll the move tree\n");
                return 1;
            }
        }
    }

    /* ---- board quality badge renders without error ---- */
    {
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        g->flipped = false;
        if (g->ply > 4) {
            g->review_cls[g->ply - 1] = RC_BRILLIANT;
        }
        gui_render(g, ren);
    }

    /* ---- opening browser renders and steps ---- */
    if (g->book && opening_line_count(g->book) > 0) {
        g->scene = SCENE_MENU;
        g->menu_index = 8;          /* Openings entry */
        g->saved.valid = false;
        SDL_Event en = {0};
        en.type = SDL_KEYDOWN;
        en.key.keysym.sym = SDLK_RETURN;
        gui_handle_event(g, &en);
        if (g->scene != SCENE_OPENINGS) {
            fprintf(stderr, "Openings menu entry did not open the browser\n");
            return 1;
        }
        gui_render(g, ren);
        SDL_Event right = {0};
        right.type = SDL_KEYDOWN;
        right.key.keysym.sym = SDLK_RIGHT;
        int step0 = g->opening_step;
        gui_handle_event(g, &right);
        gui_render(g, ren);
        if (g->opening_step < step0) {
            fprintf(stderr, "opening step did not advance\n");
            return 1;
        }

        /* browse by playing on the board: reopen to reset the path, play e4 */
        g->scene = SCENE_MENU;
        g->menu_index = 8;
        gui_handle_event(g, &en);        /* Enter re-opens with a reset path */
        gui_render(g, ren);
        int wx, wy;
        sq_window(g, ren, algebraic_to_sq("e2"), &wx, &wy);
        SDL_Event bd = {0};
        bd.type = SDL_MOUSEBUTTONDOWN;
        bd.button.button = SDL_BUTTON_LEFT;
        bd.button.x = wx; bd.button.y = wy;
        gui_handle_event(g, &bd);
        sq_window(g, ren, algebraic_to_sq("e4"), &wx, &wy);
        bd.button.x = wx; bd.button.y = wy;
        gui_handle_event(g, &bd);
        if (g->opening_path_len != 1 || strcmp(g->opening_path[0], "e4") != 0) {
            fprintf(stderr, "opening board browse did not play e4 (len=%d)\n",
                    g->opening_path_len);
            return 1;
        }
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
    } else {
        printf("(opening browser test skipped: no book)\n");
    }

    /* ---- imported PGN is shown wrapped inside the text box ---- */
    {
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        SDL_Event o = {0};
        o.type = SDL_KEYDOWN;
        o.key.keysym.sym = SDLK_o;
        o.key.keysym.mod = KMOD_LCTRL;
        gui_handle_event(g, &o);
        char longpgn[3000];
        longpgn[0] = '\0';
        for (int r = 0; r < 50; r++)
            strncat(longpgn, "1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 ",
                    sizeof longpgn - strlen(longpgn) - 1);
        snprintf(g->pgn_text, sizeof g->pgn_text, "%s", longpgn);
        g->pgn_text_len = (int)strlen(g->pgn_text);
        gui_render(g, ren);
        if (g->pgn_scroll_max <= 0) {
            fprintf(stderr, "long imported PGN did not wrap/overflow (%d)\n",
                    g->pgn_scroll_max);
            return 1;
        }
        SDL_Event esc = {0};
        esc.type = SDL_KEYDOWN;
        esc.key.keysym.sym = SDLK_ESCAPE;
        gui_handle_event(g, &esc);
    }

    /* ---- re-importing always replaces the previous tree ---- */
    {
        SDL_Event o = {0};
        o.type = SDL_KEYDOWN;
        o.key.keysym.sym = SDLK_o;
        o.key.keysym.mod = KMOD_LCTRL;
        SDL_Event en = {0};
        en.type = SDL_KEYDOWN;
        en.key.keysym.sym = SDLK_RETURN;

        gui_handle_event(g, &o);
        snprintf(g->pgn_text, sizeof g->pgn_text, "1. e4 e5 2. Nf3");
        g->pgn_text_len = (int)strlen(g->pgn_text);
        gui_handle_event(g, &en);
        if (g->ply != 3 || strcmp(g->move_san[0], "e4") != 0) {
            fprintf(stderr, "first import failed (ply=%d)\n", g->ply);
            return 1;
        }
        gui_handle_event(g, &o);
        snprintf(g->pgn_text, sizeof g->pgn_text, "1. d4 d5");
        g->pgn_text_len = (int)strlen(g->pgn_text);
        gui_handle_event(g, &en);
        if (g->ply != 2 || strcmp(g->move_san[0], "d4") != 0) {
            fprintf(stderr, "re-import did not replace the tree (ply=%d san=%s)\n",
                    g->ply, g->ply ? g->move_san[0] : "?");
            return 1;
        }
        if (g->pgn_import_open || g->pgn_text_len != 0) {
            fprintf(stderr, "text box/history not cleared after load\n");
            return 1;
        }
        if (g->review_on) {
            fprintf(stderr, "review auto-started on upload (should wait for Analyze)\n");
            return 1;
        }
    }

    /* ---- offline account creation ---- */
    {
        bool was_offline = g->offline_mode;
        bool was_local = g->account_local;
        char was_profile[40];
        snprintf(was_profile, sizeof was_profile, "%s", g->local_profile);
        g->offline_mode = true;
        g->scene = SCENE_MENU;
        g->menu_index = 9;              /* Account entry */
        g->saved.valid = false;
        SDL_Event en = {0};
        en.type = SDL_KEYDOWN;
        en.key.keysym.sym = SDLK_RETURN;
        gui_handle_event(g, &en);
        if (!g->account_open) {
            fprintf(stderr, "offline Account did not open the modal\n");
            return 1;
        }
        snprintf(g->account_user_in, sizeof g->account_user_in, "Tester");
        g->account_focus = 0;
        gui_handle_event(g, &en);
        if (!g->account_local || strcmp(g->local_profile, "Tester") != 0) {
            fprintf(stderr, "offline profile was not created\n");
            return 1;
        }
        g->account_open = false;
        g->account_local = was_local;
        g->offline_mode = was_offline;
        snprintf(g->local_profile, sizeof g->local_profile, "%s", was_profile);
    }

    /* ---- review summary screen renders and returns ---- */
    {
        g->scene = SCENE_REVIEW;
        g->rev_report = true;
        g->rev_accuracy = 88.5;
        g->rev_acpl = 42;
        memset(g->rev_count, 0, sizeof g->rev_count);
        memset(g->rev_count_side, 0, sizeof g->rev_count_side);
        g->rev_count[RC_BEST] = 7;
        g->rev_count[RC_BLUNDER] = 1;
        g->rev_count_side[0][RC_BEST] = 5;
        g->rev_count_side[1][RC_BEST] = 2;
        g->rev_count_side[0][RC_BRILLIANT] = 2;
        g->rev_count_side[0][RC_BLUNDER] = 1;
        g->rev_accuracy_side[0] = 87.3;
        g->rev_accuracy_side[1] = 94.5;
        g->rev_acpl_side[0] = 42;
        g->rev_acpl_side[1] = 18;
        snprintf(g->white_name, sizeof g->white_name, "Coach-Dante");
        snprintf(g->black_name, sizeof g->black_name, "InfinityTian");
        gui_render(g, ren);
        SDL_Event esc = {0};
        esc.type = SDL_KEYDOWN;
        esc.key.keysym.sym = SDLK_ESCAPE;
        gui_handle_event(g, &esc);
        if (g->scene != SCENE_GAME) {
            fprintf(stderr, "review screen did not return on Esc\n");
            return 1;
        }
    }

    /* ---- navigating during a review must not end it or open the report ---- */
    {
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        if (g->pgntree && g->tree_cur && g->path_len >= 2) {
            g->review_on = true;
            g->rev_report = false;
            g->review_i = 0;
            g->review_n = g->path_len;
            for (int i = 0; i < g->path_len && i < MAX_PLY; i++)
                g->review_nodes[i] = g->path_nodes[i];
            SDL_Event lk = {0};
            lk.type = SDL_KEYDOWN;
            lk.key.keysym.sym = SDLK_LEFT;
            gui_handle_event(g, &lk);
            if (!g->review_on || g->scene != SCENE_GAME) {
                fprintf(stderr, "navigation interrupted the review "
                        "(on=%d scene=%d)\n", g->review_on, g->scene);
                return 1;
            }
            gui_render(g, ren);
            g->review_on = false;
        }
    }

    /* ---- wide eval bar renders (analysis, with a value) ---- */
    {
        g->scene = SCENE_GAME;
        g->mode = MODE_ANALYSIS;
        g->eval_valid = true;
        g->eval_cp = 70;
        g->eval_mate = 0;
        g->eval_has_mate = false;
        g->eval_depth = 18;
        g->flipped = false;
        gui_render(g, ren);
        g->flipped = true;
        gui_render(g, ren);
        g->eval_valid = false;
        g->flipped = false;
    }

    /* ---- welcome online/offline pill toggles ---- */
    {
        bool was = g->offline_mode;
        g->scene = SCENE_MENU;
        g->saved.valid = false;
        gui_render(g, ren);
        SDL_Rect pill = { 256, 490, 250, 34 }; /* home play card mode switch */
        int wx, wy;
        SDL_RenderLogicalToWindow(ren, (float)(pill.x + 20), (float)(pill.y + 17),
                                  &wx, &wy);
        SDL_Event md = {0};
        md.type = SDL_MOUSEBUTTONDOWN;
        md.button.button = SDL_BUTTON_LEFT;
        md.button.x = wx; md.button.y = wy;
        gui_handle_event(g, &md);
        if (g->offline_mode == was) {
            fprintf(stderr, "mode pill did not toggle offline mode\n");
            return 1;
        }
        gui_render(g, ren);
        gui_handle_event(g, &md);       /* toggle back */
        if (g->offline_mode != was) {
            fprintf(stderr, "mode pill did not toggle back\n");
            return 1;
        }
    }

    /* ---- browser flip toggles ---- */
    if (g->book && opening_line_count(g->book) > 0) {
        g->scene = SCENE_OPENINGS;
        bool before = g->flipped;
        SDL_Event fk = {0};
        fk.type = SDL_KEYDOWN;
        fk.key.keysym.sym = SDLK_f;
        gui_handle_event(g, &fk);
        if (g->flipped == before) {
            fprintf(stderr, "browser flip did not toggle\n");
            return 1;
        }
        g->flipped = before;
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
    if (abs(x0 - (winw - x1)) > 4 || abs(y0 - (winh - y1)) > 4) {
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
