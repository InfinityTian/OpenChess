#include "gui.h"
#include "pgn.h"
#include "fen.h"
#include "paths.h"
#include "audio.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <SDL_image.h>

#define GUI_PI 3.14159265358979323846f

static void ann_color_for(Uint8 *r, Uint8 *g, Uint8 *b);
static void engine_slider_rect(const Gui *g, SDL_Rect *track);
static void engine_arrows_rect(const Gui *g, SDL_Rect *box);
static int  eng_slider_from_x(const Gui *g, int mx);
static int  clampi(int v, int lo, int hi);

/* ------------------------------------------------------------------ */
/* style registries                                                    */
/* ------------------------------------------------------------------ */

static const char *ANIM_KEYS[ANIM_STYLE_COUNT]   = { "arcade", "slide", "fade", "none" };
static const char *ANIM_LABELS[ANIM_STYLE_COUNT] = { "Arcade", "Slide", "Fade", "None" };
static const Uint32 ANIM_DURS[ANIM_STYLE_COUNT]  = { 210, 180, 170, 0 };

static ThemeEntry *cur_board(Gui *g)
{
    if (!g->boards.items || g->board_index < 0 || g->board_index >= g->boards.count)
        return NULL;
    return &g->boards.items[g->board_index];
}

static ThemeEntry *cur_piece(Gui *g)
{
    if (!g->pieces.items || g->piece_index < 0 || g->piece_index >= g->pieces.count)
        return NULL;
    return &g->pieces.items[g->piece_index];
}

/*
 * Reset to the fixed base layout. All geometry is expressed in these base
 * coordinates; `zoom` magnifies the whole canvas to fit the window.
 */
static void layout_base(Gui *g)
{
    g->sq      = SQ_SIZE;
    g->board_x = BOARD_X;
    g->board_y = BOARD_Y;
    g->panel_x = g->board_x + 8 * g->sq + 24;
    g->win_w   = g->panel_x + PANEL_W;
    g->win_h   = WIN_H;
}

/* Device pixels per base unit = display density * user zoom. */
static float eff_scale(const Gui *g)
{
    float s = g->ui_scale > 0.0f ? g->ui_scale : 1.0f;
    float z = g->zoom > 0.0f ? g->zoom : 1.0f;
    return s * z;
}

static float clamp_zoom(float z)
{
    if (z < 0.25f) z = 0.25f;
    if (z > 4.0f) z = 4.0f;
    return z;
}

/* Fit the base canvas inside a window of w x h (never crops; may letterbox). */
static float fit_zoom(float w, float h)
{
    float z = w / (float)WIN_W;
    float zy = h / (float)WIN_H;
    if (zy < z) z = zy;
    if (z < 0.25f) z = 0.25f;
    return z;
}

/* The drag handle lives at the board's bottom-right screen corner. */
static void board_grip_rect(const Gui *g, SDL_Rect *r)
{
    r->w = 18;
    r->h = 18;
    r->x = g->board_x + 8 * g->sq - r->w;
    r->y = g->board_y + 8 * g->sq - r->h;
}

/* Apply the current magnification and centre the base canvas in the window. */
static void apply_render_scale(Gui *g)
{
    if (!g->ren) return;

    /* Keep the display density live so drawing and input use the same numbers
     * (e.g. after a scaled-resolution / display change). */
    if (g->win) {
        int ow = g->win_w, oh = g->win_h, ww = g->win_w, wh = g->win_h;
        SDL_GetWindowSize(g->win, &ww, &wh);
        SDL_GetRendererOutputSize(g->ren, &ow, &oh);
        float density = (ww > 0) ? (float)ow / (float)ww : 1.0f;
        if (density < 1.0f) density = 1.0f;
        g->ui_scale = density;
    }

    float eff = eff_scale(g);

    /* Set the scale first: SDL_RenderSetViewport interprets its rect in
     * logical units (it multiplies by the current scale). */
    SDL_RenderSetScale(g->ren, eff, eff);

    int ow = g->win_w, oh = g->win_h;
    SDL_GetRendererOutputSize(g->ren, &ow, &oh);

    int cw = (int)lroundf((float)g->win_w * eff);
    int ch = (int)lroundf((float)g->win_h * eff);
    if (cw > ow) cw = ow;
    if (ch > oh) ch = oh;
    int vx = (ow - cw) / 2;
    int vy = (oh - ch) / 2;

    SDL_Rect vp = { (int)lroundf((float)vx / eff + g->pan_x),
                    (int)lroundf((float)vy / eff + g->pan_y),
                    g->win_w, g->win_h };
    SDL_RenderSetViewport(g->ren, &vp);

    /* Some SDL versions reset the scale in SetViewport: re-apply it. */
    SDL_RenderSetScale(g->ren, eff, eff);

    /* Record exactly what was installed; set_mouse inverts these values so that
     * input can never diverge from drawing (e.g. after SDL resets the viewport
     * on a window resize). */
    g->tf_scale = eff;
    g->tf_vpx = (float)vp.x;
    g->tf_vpy = (float)vp.y;
    g->tf_ux = g->tf_uy = (g->ui_scale > 0.0f ? g->ui_scale : 1.0f);
}

/* Resize the OS window so the magnified canvas fits exactly. */
static void apply_window_size(Gui *g)
{
    if (!g->win) return;
    g->board_driven_resize = true;
    SDL_SetWindowSize(g->win,
                      (int)lroundf(g->win_w * g->zoom),
                      (int)lroundf(g->win_h * g->zoom));
}

/* VSync is used when max_fps is 0 (uncapped-prevented by the display); an
 * explicit cap disables it so the frame limiter in main can go higher. */
void gui_apply_vsync(Gui *g)
{
    if (!g || !g->ren) return;
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_RenderSetVSync(g->ren, g->max_fps == 0 ? 1 : 0);
#else
    (void)g;
#endif
}

/* ---- live analysis evaluation ---- */

static void eval_stop(Gui *g)
{
    if (g->eval_ai) ai_stop_search(g->eval_ai);
    g->eval_valid = false;
}

/* (Re)start the analysis engine on the current position, honouring the
 * MultiPV/threads/hash/time/depth settings. MultiPV 0 closes the engine. */
static void eval_restart(Gui *g)
{
    if (g->eng_multipv <= 0) {
        if (g->eval_ai) { ai_stop(g->eval_ai); g->eval_ai = NULL; }
        g->eval_valid = false;
        g->eng_line_count = 0;
        return;
    }
    if (!g->engine_path[0]) { g->eval_valid = false; return; }
    if (!g->eval_ai) {
        g->eval_ai = ai_start(g->engine_path);
        if (g->eval_ai) {
            ai_set_threads(g->eval_ai, g->eng_threads);
            ai_set_hash(g->eval_ai, g->eng_hash);
        }
    }
    if (!g->eval_ai) { g->eval_valid = false; return; }

    ai_stop_search(g->eval_ai);
    ai_set_multipv(g->eval_ai, g->eng_multipv);
    g->eval_side = g->board.side;
    g->eng_line_count = 0;
    g->eval_valid = false;

    if (g->eng_depth > 0) {
        ai_set_depth(g->eval_ai, g->eng_depth);
        ai_go(g->eval_ai, &g->board);
    } else if (g->eng_time_ms > 0) {
        ai_set_depth(g->eval_ai, 0);
        ai_set_movetime(g->eval_ai, g->eng_time_ms);
        ai_go(g->eval_ai, &g->board);
    } else {
        ai_set_depth(g->eval_ai, 0);
        ai_go_infinite(g->eval_ai, &g->board);
    }
}

/* Re-analyse after the position changed (analysis mode only). */
static void analysis_refresh(Gui *g)
{
    if (g->mode == MODE_ANALYSIS && g->scene == SCENE_GAME)
        eval_restart(g);
}

/* filenames indexed by Piece (WP..BK) */
static const char *PIECE_FILE[16] = {
    NULL, "wp", "wn", "wb", "wr", "wq", "wk",
    "bp", "bn", "bb", "br", "bq", "bk"
};

/* ------------------------------------------------------------------ */
/* static helpers                                                      */
/* ------------------------------------------------------------------ */

static const char *find_font(void)
{
    static const char *candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/Library/Fonts/Arial Unicode.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        FILE *f = fopen(candidates[i], "rb");
        if (f) { fclose(f); return candidates[i]; }
    }
    return NULL;
}

static SDL_Texture *make_piece_tex(SDL_Renderer *ren, TTF_Font *font,
                                   const char *glyph, SDL_Color fill,
                                   SDL_Color outline, int oz)
{
    SDL_Surface *fill_s  = TTF_RenderUTF8_Blended(font, glyph, fill);
    SDL_Surface *out_s   = TTF_RenderUTF8_Blended(font, glyph, outline);
    if (!fill_s || !out_s) { if (fill_s) SDL_FreeSurface(fill_s); if (out_s) SDL_FreeSurface(out_s); return NULL; }

    int min_x = fill_s->w, min_y = fill_s->h, max_x = -1, max_y = -1;
    SDL_LockSurface(fill_s);
    for (int y = 0; y < fill_s->h; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)fill_s->pixels + y * fill_s->pitch);
        for (int x = 0; x < fill_s->w; x++) {
            Uint8 a;
            SDL_GetRGBA(row[x], fill_s->format, NULL, NULL, NULL, &a);
            if (a > 0) {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    SDL_UnlockSurface(fill_s);

    int dx_off = 0, dy_off = 0;
    if (max_x >= min_x) {
        dx_off = fill_s->w / 2 - (min_x + max_x) / 2;
        dy_off = fill_s->h / 2 - (min_y + max_y) / 2;
    }

    int gx = oz + dx_off, gy = oz + dy_off;
    int cw = gx + fill_s->w + oz;
    int ch = gy + fill_s->h + oz;
    if (gx < oz) { cw += oz - gx; gx = oz; }
    if (gy < oz) { ch += oz - gy; gy = oz; }

    SDL_Surface *canvas = SDL_CreateRGBSurfaceWithFormat(0, cw, ch, 32, SDL_PIXELFORMAT_RGBA32);
    if (!canvas) { SDL_FreeSurface(fill_s); SDL_FreeSurface(out_s); return NULL; }

    SDL_Rect r = { gx, gy, fill_s->w, fill_s->h };
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            SDL_Rect o = { r.x + dx, r.y + dy, r.w, r.h };
            SDL_BlitSurface(out_s, NULL, canvas, &o);
        }
    SDL_BlitSurface(fill_s, NULL, canvas, &r);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, canvas);
    SDL_FreeSurface(fill_s); SDL_FreeSurface(out_s); SDL_FreeSurface(canvas);
    return tex;
}

static void set_render_color(SDL_Renderer *ren, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
}

static void draw_rect(SDL_Renderer *ren, SDL_Rect *rc, Uint8 r, Uint8 g, Uint8 b, bool fill)
{
    set_render_color(ren, r, g, b);
    if (fill) SDL_RenderFillRect(ren, rc);
    else SDL_RenderDrawRect(ren, rc);
}

static void render_text(Gui *g, TTF_Font *font, const char *txt,
                        int x, int y, SDL_Color col)
{
    if (!font || !txt || !*txt) return;
    SDL_Renderer *ren = g->ren;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, txt, col);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    float sc = eff_scale(g);
    SDL_Rect rc = { x, y, (int)lroundf(s->w / sc), (int)lroundf(s->h / sc) };
    SDL_RenderCopy(ren, t, NULL, &rc);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

/* Map a board square (rank0=rank1) to a screen column/row (0..7), honoring
 * the current board orientation. */
static void sq_screen(const Gui *g, int sq, int *col, int *row)
{
    int file = sq % 8, rank = sq / 8;
    if (g->flipped) { *col = 7 - file; *row = rank; }
    else            { *col = file;     *row = 7 - rank; }
}

/* Inverse of sq_screen; returns -1 when the cell is outside the board. */
static int screen_sq(const Gui *g, int col, int row)
{
    if (col < 0 || col > 7 || row < 0 || row > 7) return -1;
    int file, rank;
    if (g->flipped) { file = 7 - col; rank = row; }
    else            { file = col;     rank = 7 - row; }
    return rank * 8 + file;
}

/* Draw text horizontally centered on `cx` (logical coordinates). */
static void render_text_centered(Gui *g, TTF_Font *font, const char *txt,
                                 int cx, int y, SDL_Color col)
{
    if (!font || !txt || !*txt) return;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, txt, &w, &h) != 0) return;
    float sc = eff_scale(g);
    int lw = (int)lroundf(w / sc);
    render_text(g, font, txt, cx - lw / 2, y, col);
}

/* Logical height of a rendered line, for vertical centering. */
static int text_height(const Gui *g, TTF_Font *font)
{
    if (!font) return 0;
    float sc = eff_scale(g);
    return (int)lroundf((float)TTF_FontHeight(font) / sc);
}

static int window_sq(const Gui *g, int sq, SDL_Rect *out)
{
    int col, row;
    sq_screen(g, sq, &col, &row);
    out->x = g->board_x + col * g->sq;
    out->y = g->board_y + row * g->sq;
    out->w = g->sq;
    out->h = g->sq;
    return 0;
}

static int sq_from_pos(const Gui *g, int px, int py)
{
    if (px < g->board_x || py < g->board_y) return -1;
    int col = (px - g->board_x) / g->sq;
    int row = (py - g->board_y) / g->sq;
    return screen_sq(g, col, row);
}

static bool color_of_piece(Piece p) { return WHITE_PIECE(p); }

static void piece_center(const Gui *g, int sq, float *cx, float *cy)
{
    int col, row;
    sq_screen(g, sq, &col, &row);
    *cx = g->board_x + col * g->sq + g->sq / 2.0f;
    *cy = g->board_y + row * g->sq + g->sq / 2.0f;
}

static bool pt_in(const SDL_Rect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

/* Record a window position and map it into base coordinates. */
static void set_mouse(Gui *g, int ex, int ey)
{
    g->mouse_evt.x = ex;
    g->mouse_evt.y = ey;

    if (!g->ren || !g->win) {
        float z = g->zoom > 0.0f ? g->zoom : 1.0f;
        g->mouse_win.x = ex;
        g->mouse_win.y = ey;
        g->mouse.x = (int)lroundf((float)ex / z);
        g->mouse.y = (int)lroundf((float)ey / z);
        return;
    }

    /* SDL's window-relative event coordinates can be stale after a macOS
     * zoom/move (its cached window origin is not updated), which offsets the
     * cursor. global_cursor - window_position gives the true content-relative
     * position (they are queried live), so use that. Hidden windows
     * (tests/headless) keep the event coordinates. */
    int x = ex, y = ey;
    if (!(SDL_GetWindowFlags(g->win) & SDL_WINDOW_HIDDEN)) {
        int gx = 0, gy = 0, wx = 0, wy = 0;
        SDL_GetGlobalMouseState(&gx, &gy);
        SDL_GetWindowPosition(g->win, &wx, &wy);
        g->mouse_global.x = gx;
        g->mouse_global.y = gy;
        g->win_pos.x = wx;
        g->win_pos.y = wy;
        if (gx || gy) { x = gx - wx; y = gy - wy; }
    }
    g->mouse_win.x = x;
    g->mouse_win.y = y;

    /* Re-install the intended transform first (SDL resets the viewport on
     * window resize), then invert exactly those stored values:
     *   physical_device = viewport + base * scale
     *   physical_device = window_point * (device / point)
     * Because these are the same numbers apply_render_scale just used, input
     * can never drift from what is drawn. */
    apply_render_scale(g);

    float s  = g->tf_scale > 0.0f ? g->tf_scale : 1.0f;
    float ux = g->tf_ux > 0.0f ? g->tf_ux : 1.0f;
    float uy = g->tf_uy > 0.0f ? g->tf_uy : 1.0f;

    g->mouse.x = (int)lroundf((float)x * ux / s - g->tf_vpx);
    g->mouse.y = (int)lroundf((float)y * uy / s - g->tf_vpy);
}

/* ------------------------------------------------------------------ */
/* asset loading                                                       */
/* ------------------------------------------------------------------ */

static void destroy_piece_textures(Gui *g)
{
    for (int i = 0; i < 16; i++) {
        if (g->tex_piece[i]) SDL_DestroyTexture(g->tex_piece[i]);
        g->tex_piece[i] = NULL;
    }
}

static void build_glyph_textures(Gui *g)
{
    if (!g->font_piece[0] || !g->ren) return;
    static const char *glyph[16] = {
        "", "\xE2\x99\x99", "\xE2\x99\x98", "\xE2\x99\x97",
        "\xE2\x99\x96", "\xE2\x99\x95", "\xE2\x99\x94",
        "\xE2\x99\x9F", "\xE2\x99\x9E", "\xE2\x99\x9D",
        "\xE2\x99\x9C", "\xE2\x99\x9B", "\xE2\x99\x9A"
    };
    SDL_Color white = {240,240,240,255}, dark = {20,20,20,255},
              outline_dark = {35,35,40,255}, outline_lite = {235,235,240,255};
    int oz = (int)lroundf(4.0f * g->ui_scale);
    if (oz < 1) oz = 1;
    for (int p = WP; p <= BK; p++) {
        if (WHITE_PIECE(p))
            g->tex_piece[p] = make_piece_tex(g->ren, g->font_piece[0], glyph[p], white, outline_dark, oz);
        else
            g->tex_piece[p] = make_piece_tex(g->ren, g->font_piece[0], glyph[p], dark, outline_lite, oz);
    }
}

static void load_piece_style(Gui *g)
{
    destroy_piece_textures(g);
    ThemeEntry *pt = cur_piece(g);
    if (pt && pt->path[0] && g->ren) {
        bool ok = true;
        for (int p = WP; p <= BK; p++) {
            char path[THEME_PATH_MAX + 32];
            snprintf(path, sizeof path, "%s/%s.png", pt->path, PIECE_FILE[p]);
            SDL_Texture *t = IMG_LoadTexture(g->ren, path);
            if (!t) { ok = false; break; }
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            g->tex_piece[p] = t;
        }
        if (ok) return;
        fprintf(stderr, "piece image load failed for '%s': %s\n", pt->path, IMG_GetError());
        destroy_piece_textures(g);
    }
    build_glyph_textures(g);
}

/* Reopen every font at the current magnification and rebuild the piece
 * textures (needed after a zoom change; image sets are reloaded harmlessly). */
static void rebuild_fonts(Gui *g)
{
    if (g->font_piece[0]) TTF_CloseFont(g->font_piece[0]);
    if (g->font_piece[1]) TTF_CloseFont(g->font_piece[1]);
    if (g->font_ui) TTF_CloseFont(g->font_ui);
    if (g->font_small) TTF_CloseFont(g->font_small);
    if (g->font_tiny) TTF_CloseFont(g->font_tiny);
    g->font_piece[0] = g->font_piece[1] = NULL;
    g->font_ui = g->font_small = g->font_tiny = NULL;

    const char *fpath = find_font();
    if (fpath) {
        float sc = eff_scale(g);
        g->font_piece[0] = TTF_OpenFont(fpath, (int)lroundf(SQ_SIZE * sc));
        g->font_piece[1] = TTF_OpenFont(fpath, (int)lroundf(SQ_SIZE * sc));
        g->font_ui      = TTF_OpenFont(fpath, (int)lroundf(22 * sc));
        g->font_small   = TTF_OpenFont(fpath, (int)lroundf(15 * sc));
        g->font_tiny    = TTF_OpenFont(fpath, (int)lroundf(12 * sc));
        if (!g->font_piece[0] || !g->font_piece[1] || !g->font_ui ||
            !g->font_small || !g->font_tiny)
            fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
    }

    /* Only the glyph set depends on the font size; image piece textures scale. */
    ThemeEntry *pt = cur_piece(g);
    if (!pt || !pt->path[0]) load_piece_style(g);
}

static void load_board_style(Gui *g)
{
    if (g->tex_board) { SDL_DestroyTexture(g->tex_board); g->tex_board = NULL; }
    ThemeEntry *bt = cur_board(g);
    if (bt && bt->path[0] && g->ren) {
        SDL_Texture *t = IMG_LoadTexture(g->ren, bt->path);
        if (t) {
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            g->tex_board = t;
        } else {
            fprintf(stderr, "board image load failed: %s (%s)\n", bt->path, IMG_GetError());
        }
    }
}

/* ------------------------------------------------------------------ */
/* game state helpers                                                  */
/* ------------------------------------------------------------------ */

/* Drop the current piece selection and its valid-target markers. */
static void clear_selection(Gui *g)
{
    g->selected = -1;
    g->targets.count = 0;
}

static void ann_clear(Gui *g)
{
    g->ann_square_count = 0;
    g->ann_arrow_count = 0;
    g->ann_dragging = false;
    g->ann_from = g->ann_to = -1;
}

static Move *target_move(Gui *g, int to)
{
    for (int i = 0; i < g->targets.count; i++) {
        Move m = g->targets.moves[i];
        if (MOVE_TO(m) == to) return &g->targets.moves[i];
    }
    return NULL;
}

static void recompute_state(Gui *g)
{
    g->state = game_state(&g->board);
}

static void clear_anims(Gui *g)
{
    g->anim_count = 0;
}

/* Keep the FEN field in sync with the board unless the user is editing it. */
static void fen_refresh(Gui *g)
{
    if (g->fen_active) return;
    fen_generate(&g->board, g->fen_text, sizeof g->fen_text);
    g->fen_len = (int)strlen(g->fen_text);
}

/* Hide and clear the on-demand SAN entry box. */
static void san_close_box(Gui *g)
{
    g->san_open = false;
    g->input.active = false;
    g->input.len = 0;
    g->input.text[0] = '\0';
}

static void reset_board_state(Gui *g)
{
    clear_selection(g);
    ann_clear(g);
    g->promo_from = g->promo_to = -1;
    clear_anims(g);
    san_close_box(g);
    recompute_state(g);
}

/* Replace the position from a FEN string. Returns false on parse failure. */
static bool load_fen(Gui *g, const char *fen)
{
    Board b;
    if (!fen_parse(fen, &b)) return false;
    g->board = b;
    g->ply = 0;
    reset_board_state(g);
    fen_refresh(g);
    analysis_refresh(g);
    return true;
}

/* Describe the visual transition for one move. Board state is already updated
 * by the caller; `pre` is the position before the move. */
static void enqueue_move_anim(Gui *g, const Board *pre, Move m)
{
    if (g->anim_style == ANIM_NONE) return;
    if (g->anim_count >= ANIM_QUEUE_MAX) return;

    AnimStep s;
    memset(&s, 0, sizeof s);
    s.piece  = pre->board[MOVE_FROM(m)];
    s.from   = MOVE_FROM(m);
    s.to     = MOVE_TO(m);
    s.cap_sq = -1;

    uint32_t fl = MOVE_FLAGS(m);
    if (fl & FLAG_EP) {
        int csq = (MOVE_TO(m) & 7) | (MOVE_FROM(m) & ~7);
        s.captured = pre->board[csq];
        s.cap_sq   = csq;
    } else if (pre->board[MOVE_TO(m)] != EMPTY) {
        s.captured = pre->board[MOVE_TO(m)];
        s.cap_sq   = MOVE_TO(m);
    }

    if (fl & FLAG_CASTLE_K) {
        s.has_rook = true;
        s.rook  = pre->board[MOVE_TO(m) + 1];
        s.rfrom = MOVE_TO(m) + 1;
        s.rto   = MOVE_TO(m) - 1;
    } else if (fl & FLAG_CASTLE_Q) {
        s.has_rook = true;
        s.rook  = pre->board[MOVE_TO(m) - 2];
        s.rfrom = MOVE_TO(m) - 2;
        s.rto   = MOVE_TO(m) + 1;
    }

    s.dur     = ANIM_DURS[g->anim_style];
    s.started = false;
    s.start   = 0;
    g->anim_queue[g->anim_count++] = s;
}

/* Pick and play the sound for a move that was just applied (g->board is the
 * position after it, `pre` the one before). */
static void play_move_sound(Gui *g, const Board *pre, Move m)
{
    uint32_t fl = MOVE_FLAGS(m);
    Color mover = pre->side;

    if (g->state == CHECKMATE) {
        if (g->mode == MODE_ANALYSIS) { audio_play(SND_GAME_END); return; }
        Color winner = (mover == WHITE) ? BLACK : WHITE;
        Color hero = (g->mode == MODE_SINGLE) ? g->human_color : g->local_color;
        audio_play(winner == hero ? SND_GAME_WIN : SND_GAME_LOSE);
        return;
    }
    if (g->state == STALEMATE || g->state == INSUFFICIENT_MATERIAL) {
        audio_play(SND_GAME_DRAW);
        return;
    }
    if (in_check(&g->board, g->board.side)) { audio_play(SND_CHECK); return; }
    if (fl & (FLAG_CASTLE_K | FLAG_CASTLE_Q)) { audio_play(SND_CASTLE); return; }
    if (fl & FLAG_PROMO) { audio_play(SND_PROMOTE); return; }
    if ((fl & (FLAG_CAPTURE | FLAG_EP)) || pre->board[MOVE_TO(m)] != EMPTY) {
        audio_play(SND_CAPTURE);
        return;
    }

    bool self = true;
    if (g->mode == MODE_SINGLE)      self = (mover == g->human_color);
    else if (g->mode == MODE_LOCAL)  self = (mover == g->local_color);
    audio_play(self ? SND_MOVE_SELF : SND_MOVE_OPPONENT);
}

static void push_move(Gui *g, Move m)
{
    if (g->ply >= MAX_PLY) return;
    Board pre = g->board;
    g->before[g->ply] = pre;
    move_to_san(&g->board, m, g->last_san, sizeof g->last_san);
    snprintf(g->move_san[g->ply], 8, "%s", g->last_san);
    enqueue_move_anim(g, &pre, m);
    make_move_plumb(&g->board, m);
    g->history[g->ply] = m;
    g->ply++;
    clear_selection(g);
    ann_clear(g);
    g->promo_from = g->promo_to = -1;
    san_close_box(g);
    recompute_state(g);
    play_move_sound(g, &pre, m);
    fen_refresh(g);
    analysis_refresh(g);
}

static void set_msg(Gui *g, const char *fmt, const char *arg)
{
    if (arg)
        snprintf(g->msg, sizeof g->msg, fmt, arg);
    else
        snprintf(g->msg, sizeof g->msg, "%s", fmt);
    g->msg_until = SDL_GetTicks() + 2500;
}

static void do_undo(Gui *g)
{
    /* Singleplayer rewinds to the human's previous turn. */
    if (g->mode == MODE_SINGLE) {
        if (g->ai_thinking) { ai_stop_search(g->ai); g->ai_thinking = false; }
        while (g->ply > 0) {
            g->ply--;
            g->board = g->before[g->ply];
            if (g->board.side == g->human_color) break;
        }
        reset_board_state(g);
        fen_refresh(g);
        return;
    }

    if (g->ply > 0) {
        g->ply--;
        g->board = g->before[g->ply];
        reset_board_state(g);
        fen_refresh(g);
        analysis_refresh(g);
    }
}

static void do_restart(Gui *g)
{
    if (g->ai_thinking) { ai_stop_search(g->ai); g->ai_thinking = false; }
    board_reset(&g->board);
    g->ply = 0;
    g->input.len = 0;
    g->input.text[0] = 0;
    g->msg[0] = 0;
    reset_board_state(g);
    fen_refresh(g);
    if (g->mode == MODE_SINGLE && g->ai) ai_new_game(g->ai);
    analysis_refresh(g);
}

/* Try to apply typed SAN text. Returns true on success. */
static bool apply_text_san(Gui *g)
{
    char buf[64];
    int n = 0;
    for (int i = 0; i < g->input.len && i < (int)sizeof buf - 1; i++)
        buf[n++] = g->input.text[i];
    buf[n] = 0;
    while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = 0;

    if (n == 0) return false;

    Move m;
    if (san_find(&g->board, buf, &m)) {
        push_move(g, m);
        g->input.len = 0;
        g->input.text[0] = 0;
        set_msg(g, "Moved %s", g->last_san);
        return true;
    }
    audio_play(SND_ILLEGAL);
    set_msg(g, "Illegal move: %s", buf);
    return false;
}

/* Typed SAN is only allowed for the side you control. */
static bool san_can_type(const Gui *g)
{
    switch (g->mode) {
        case MODE_SINGLE: return g->board.side == g->human_color;
        case MODE_LOCAL:  return g->board.side == g->local_color;
        case MODE_ANALYSIS:
        default:          return true;
    }
}

/* Reveal the SAN entry box (Enter while playing). */
static void san_open_box(Gui *g)
{
    if (g->state != NO_GAME_OVER) return;
    if (!san_can_type(g)) {
        set_msg(g, "Not your turn", NULL);
        return;
    }
    g->san_open = true;
    g->input.active = true;
    g->input.len = 0;
    g->input.text[0] = '\0';
}

/* Select a piece (own side) at sq, computing legal target squares. */
static void select_square(Gui *g, int sq)
{
    clear_selection(g);
    Piece p = g->board.board[sq];
    if (p == EMPTY) return;
    bool my_piece = (g->board.side == WHITE) ? WHITE_PIECE(p) : BLACK_PIECE(p);
    if (!my_piece) return;
    if (g->state != NO_GAME_OVER) return;

    g->selected = sq;
    MoveList legal;
    gen_legal(&g->board, &legal);
    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        if (MOVE_FROM(m) == sq && g->targets.count < MAX_MOVES)
            g->targets.moves[g->targets.count++] = m;
    }
}

/* Try to move selected piece to `to`. If the move requires a promotion
 * choice, open the chooser instead. */
static void try_move_to(Gui *g, int to)
{
    if (g->selected < 0) return;
    Move *m = target_move(g, to);
    if (!m) return;
    if (MOVE_FLAGS(*m) & FLAG_PROMO) {
        g->promo_from = MOVE_FROM(*m);
        g->promo_to = MOVE_TO(*m);
        clear_selection(g);
        return;
    }
    push_move(g, *m);
}

/* Resolve the pending promotion with the chosen piece code. */
static void resolve_promotion(Gui *g, Piece promo)
{
    if (g->promo_from < 0) return;
    int from = g->promo_from;
    int to = g->promo_to;
    g->promo_from = g->promo_to = -1;
    MoveList legal;
    gen_legal(&g->board, &legal);
    Piece w = color_of_piece(g->board.board[from]) ? promo : (Piece)(promo + (BP - WN));
    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        if (MOVE_FROM(m) == from && MOVE_TO(m) == to &&
            (MOVE_FLAGS(m) & FLAG_PROMO) && MOVE_PROMO(m) == w) {
            push_move(g, m);
            return;
        }
    }
}

static void btn_rects(const Gui *g, SDL_Rect *undo, SDL_Rect *restart)
{
    undo->x = g->panel_x;          undo->y = g->win_h - 120; undo->w = 80; undo->h = 40;
    restart->x = g->panel_x + 88;  restart->y = g->win_h - 120; restart->w = 80; restart->h = 40;
}

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* MultiPV slider in the analysis panel (0 = engine off .. AI_MAX_LINES). */
static void engine_slider_rect(const Gui *g, SDL_Rect *track)
{
    track->x = g->panel_x + 96;
    track->y = g->board_y + 56;
    track->w = 220;
    track->h = 14;
}

static int eng_slider_from_x(const Gui *g, int mx)
{
    SDL_Rect t;
    engine_slider_rect(g, &t);
    int v = (int)lroundf((float)(mx - t.x) / (float)t.w * (float)AI_MAX_LINES);
    return clampi(v, 0, AI_MAX_LINES);
}

/* "Arrows" checkbox on the Engine-lines label row. */
static void engine_arrows_rect(const Gui *g, SDL_Rect *box)
{
    box->x = g->panel_x + 78;
    box->y = g->board_y + 52;
    box->w = 16;
    box->h = 16;
}

static void styles_btn_rect(const Gui *g, SDL_Rect *out)
{
    out->x = g->panel_x + 176; out->y = g->win_h - 120; out->w = 80; out->h = 40;
}

static void pgn_btn_rect(const Gui *g, SDL_Rect *out)
{
    out->x = g->panel_x + 264; out->y = g->win_h - 120; out->w = 80; out->h = 40;
}

static void input_rect(const Gui *g, SDL_Rect *out)
{
    out->x = g->panel_x; out->y = g->win_h - 200; out->w = 320; out->h = 40;
}

static void menu_btn_rect(const Gui *g, SDL_Rect *out)
{
    out->x = g->panel_x + 352; out->y = g->win_h - 120; out->w = 80; out->h = 40;
}

/* FEN load/copy controls sit between the move list and the HUD. */
static void fen_rects(const Gui *g, SDL_Rect *field, SDL_Rect *load, SDL_Rect *copy)
{
    field->x = g->panel_x;       field->y = g->board_y + 420; field->w = 252; field->h = 34;
    load->x  = g->panel_x + 260; load->y  = field->y;       load->w  = 54;  load->h  = 34;
    copy->x  = g->panel_x + 320; copy->y  = field->y;       copy->w  = 54;  copy->h  = 34;
}

/* ------------------------------------------------------------------ */
/* scenes                                                              */
/* ------------------------------------------------------------------ */

static void open_appearance(Gui *g, Scene return_to);
static void open_settings_scene(Gui *g, Scene return_to);
static void handle_settings_keydown(Gui *g, const SDL_KeyboardEvent *ke);
static void handle_settings_mousedown(Gui *g);

#define MENU_COUNT 6
static const char *MENU_ITEMS[MENU_COUNT] = {
    "Singleplayer (vs AI)",
    "Analysis (both sides)",
    "Local Multiplayer",
    "Appearance (boards & pieces)",
    "Settings",
    "Quit",
};

/* Local Multiplayer is enabled only when SDL_net is compiled in. */
static bool menu_enabled(int i)
{
    if (i == 2) return net_available();
    return i >= 0 && i < MENU_COUNT;
}

/* A "Continue" entry is prepended while a resumable game exists. */
static int menu_count(const Gui *g)
{
    return MENU_COUNT + (g->saved.valid ? 1 : 0);
}

static int menu_base_index(const Gui *g, int i)
{
    return g->saved.valid ? i - 1 : i;
}

static const char *menu_label(const Gui *g, int i)
{
    if (g->saved.valid && i == 0) return "Continue";
    int b = menu_base_index(g, i);
    return (b >= 0 && b < MENU_COUNT) ? MENU_ITEMS[b] : "";
}

static bool menu_item_enabled(const Gui *g, int i)
{
    if (g->saved.valid && i == 0) return true;
    return menu_enabled(menu_base_index(g, i));
}

/* Difficulty presets: engine skill (0..20) and think time in ms. */
typedef struct { const char *label; int skill; int movetime; } AiLevel;
static const AiLevel AI_LEVELS[3] = {
    { "Easy",   0,  150 },
    { "Medium", 10, 400 },
    { "Hard",   20, 1000 },
};
#define AI_LEVEL_COUNT 3

#define MENU_TOP    200     /* below the title/subtitle */
#define MENU_BOTTOM 110     /* reserved above the window bottom */
#define MENU_ITEM_H 46
#define MENU_GAP    12

/* Responsive menu entries: fixed-size buttons centred vertically in the area
 * between the subtitle and the footer, never overlapping either. */
static void menu_item_rect(const Gui *g, int i, SDL_Rect *r)
{
    r->w = 440;
    if (r->w > g->win_w - 160) r->w = g->win_w - 160;
    if (r->w < 240) r->w = 240;
    r->h = MENU_ITEM_H;
    r->x = (g->win_w - r->w) / 2;

    int n = menu_count(g);
    int total = n * r->h + (n - 1) * MENU_GAP;
    int area  = (g->win_h - MENU_BOTTOM) - MENU_TOP;
    int start = MENU_TOP + (area - total) / 2;
    if (start < MENU_TOP) start = MENU_TOP;
    r->y = start + i * (r->h + MENU_GAP);
}

static void start_analysis(Gui *g)
{
    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    g->flipped = false;
    g->auto_flip = false;
    snprintf(g->white_name, sizeof g->white_name, "White");
    snprintf(g->black_name, sizeof g->black_name, "Black");
    g->menu_msg[0] = 0;
    g->msg[0] = 0;
    g->input.len = 0;
    g->input.text[0] = 0;
    g->input.active = false;    /* SAN entry appears on Enter */
    g->san_open = false;
    g->fen_active = false;
    board_reset(&g->board);
    g->ply = 0;
    reset_board_state(g);
    fen_refresh(g);
    eval_restart(g);
    g->saved.valid = false;
}

/* Snapshot the current game so the menu can offer to resume it. Network games
 * cannot be resumed (the peer is gone), so they clear the snapshot instead. */
static void save_game(Gui *g)
{
    if (g->mode == MODE_LOCAL) { g->saved.valid = false; return; }

    SavedGame *s = &g->saved;
    s->mode        = g->mode;
    s->board       = g->board;
    memcpy(s->before, g->before, sizeof s->before);
    memcpy(s->history, g->history, sizeof s->history);
    s->ply         = g->ply;
    s->state       = g->state;
    snprintf(s->last_san, sizeof s->last_san, "%s", g->last_san);
    s->flipped     = g->flipped;
    s->auto_flip   = g->auto_flip;
    s->human_color = g->human_color;
    s->ai_skill    = g->ai_skill;
    s->ai_movetime = g->ai_movetime;
    s->setup_side  = g->setup_side;
    s->setup_level = g->setup_level;
    snprintf(s->white_name, sizeof s->white_name, "%s", g->white_name);
    snprintf(s->black_name, sizeof s->black_name, "%s", g->black_name);
    memcpy(s->move_san, g->move_san, sizeof s->move_san);
    s->valid = true;
}

/* Restore the snapshot saved by save_game() and re-enter the game screen. */
static void resume_game(Gui *g)
{
    if (!g->saved.valid) return;

    SavedGame *s = &g->saved;
    eval_stop(g);
    g->mode        = s->mode;
    g->board       = s->board;
    memcpy(g->before, s->before, sizeof g->before);
    memcpy(g->history, s->history, sizeof g->history);
    g->ply         = s->ply;
    g->state       = s->state;
    snprintf(g->last_san, sizeof g->last_san, "%s", s->last_san);
    g->flipped     = s->flipped;
    g->auto_flip   = s->auto_flip;
    g->human_color = s->human_color;
    g->ai_skill    = s->ai_skill;
    g->ai_movetime = s->ai_movetime;
    g->setup_side  = s->setup_side;
    g->setup_level = s->setup_level;
    snprintf(g->white_name, sizeof g->white_name, "%s", s->white_name);
    snprintf(g->black_name, sizeof g->black_name, "%s", s->black_name);
    memcpy(g->move_san, s->move_san, sizeof g->move_san);

    g->scene       = SCENE_GAME;
    g->ai_thinking = false;
    g->fen_active  = false;
    g->msg[0]      = 0;
    g->menu_msg[0] = 0;
    reset_board_state(g);
    fen_refresh(g);

    if (g->mode == MODE_SINGLE) {
        if (!g->ai && g->engine_path[0]) g->ai = ai_start(g->engine_path);
        if (g->ai) {
            ai_set_skill(g->ai, g->ai_skill);
            ai_set_movetime(g->ai, g->ai_movetime);
            ai_new_game(g->ai);
        }
    } else {
        analysis_refresh(g);
    }
}

static void go_to_menu(Gui *g)
{
    save_game(g);
    clear_selection(g);
    g->promo_from = g->promo_to = -1;
    g->fen_active = false;
    g->input.active = false;
    g->msg[0] = 0;
    if (g->ai_thinking) {
        ai_stop_search(g->ai);
        g->ai_thinking = false;
    }
    if (g->net) {
        if (net_state(g->net) == NET_STATE_CONNECTED) net_send(g->net, "BYE");
        net_close(g->net);
        g->net = NULL;
    }
    g->net_waiting = false;
    g->net_sent_ply = 0;
    eval_stop(g);
    g->menu_index = g->saved.valid ? 0 : 1;
    g->scene = SCENE_MENU;
}

static void start_single(Gui *g)
{
    eval_stop(g);
    g->scene = SCENE_GAME;
    g->mode = MODE_SINGLE;
    g->human_color = (g->setup_side == 1) ? BLACK : WHITE;
    if (g->human_color == WHITE) {
        snprintf(g->white_name, sizeof g->white_name, "You");
        snprintf(g->black_name, sizeof g->black_name, "Stockfish");
    } else {
        snprintf(g->white_name, sizeof g->white_name, "Stockfish");
        snprintf(g->black_name, sizeof g->black_name, "You");
    }
    g->auto_flip = true;
    g->flipped = (g->human_color == BLACK);
    g->ai_skill = AI_LEVELS[g->setup_level].skill;
    g->ai_movetime = AI_LEVELS[g->setup_level].movetime;
    g->menu_msg[0] = 0;
    g->msg[0] = 0;
    g->input.len = 0;
    g->input.text[0] = 0;
    g->input.active = false;
    g->fen_active = false;
    board_reset(&g->board);
    g->ply = 0;
    reset_board_state(g);
    fen_refresh(g);
    g->ai_thinking = false;

    if (!g->ai && g->engine_path[0])
        g->ai = ai_start(g->engine_path);
    if (g->ai) {
        ai_set_skill(g->ai, g->ai_skill);
        ai_set_movetime(g->ai, g->ai_movetime);
        ai_new_game(g->ai);
    } else {
        set_msg(g, "Stockfish not found", NULL);
    }
    g->saved.valid = false;
}

static void enter_local_game(Gui *g, Color side)
{
    eval_stop(g);
    g->scene = SCENE_GAME;
    g->mode = MODE_LOCAL;
    snprintf(g->white_name, sizeof g->white_name, "Host");
    snprintf(g->black_name, sizeof g->black_name, "Guest");
    g->local_color = side;
    g->auto_flip = true;
    g->flipped = (side == BLACK);
    g->menu_msg[0] = 0;
    g->msg[0] = 0;
    g->input.len = 0;
    g->input.text[0] = 0;
    g->input.active = false;
    g->fen_active = false;
    board_reset(&g->board);
    g->ply = 0;
    reset_board_state(g);
    fen_refresh(g);
    g->net_sent_ply = 0;
    g->net_waiting = false;

    if (g->net && net_state(g->net) == NET_STATE_CONNECTED) {
        char hello[32];
        snprintf(hello, sizeof hello, "HELLO CHESS1 %s",
                 side == WHITE ? "white" : "black");
        net_send(g->net, hello);

        char fen[FEN_MAX];
        fen_generate(&g->board, fen, sizeof fen);
        char fbuf[FEN_MAX + 8];
        snprintf(fbuf, sizeof fbuf, "FEN %s", fen);
        net_send(g->net, fbuf);

        set_msg(g, side == WHITE ? "Connected - you are White"
                                 : "Connected - you are Black", NULL);
    }
    g->saved.valid = false;
}

static void promo_rects(Gui *g, SDL_Rect out[4])
{
    SDL_Rect sqr;
    window_sq(g, g->promo_to, &sqr);
    int y0 = sqr.y - 90;
    if (y0 < g->board_y) y0 = sqr.y + g->sq + 10;
    int x0 = sqr.x - 8;
    if (x0 < g->board_x) x0 = sqr.x;
    if (x0 + 4 * 54 > g->board_x + 8 * g->sq) x0 = g->board_x + 8 * g->sq - 4 * 54;
    out[0] = (SDL_Rect){x0, y0, 50, 46};
    out[1] = (SDL_Rect){x0 + 54, y0, 50, 46};
    out[2] = (SDL_Rect){x0 + 108, y0, 50, 46};
    out[3] = (SDL_Rect){x0 + 162, y0, 50, 46};
}

/* ------------------------------------------------------------------ */
/* config                                                              */
/* ------------------------------------------------------------------ */

static void trim(char *s)
{
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

static void lower_in_place(char *s)
{
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static void gui_apply_style_names(Gui *g, const char *board, const char *pieces,
                                  const char *anim)
{
    if (board) {
        int i = themes_index_of(&g->boards, board);
        if (i >= 0) g->board_index = i;
    }
    if (pieces) {
        int i = themes_index_of(&g->pieces, pieces);
        if (i >= 0) g->piece_index = i;
    }
    if (anim) {
        for (int i = 0; i < ANIM_STYLE_COUNT; i++)
            if (strcmp(anim, ANIM_KEYS[i]) == 0) g->anim_style = (AnimStyle)i;
    }
}

void gui_load_config(Gui *g, const char *path)
{
    if (path && *path) snprintf(g->config_path, sizeof g->config_path, "%s", path);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];
    char vboard[64] = "", vpieces[64] = "", vanim[64] = "", vengine[512] = "";
    char vboardsize[16] = "";
    char vthreads[16] = "", vhash[16] = "", vmultipv[16] = "";
    char vtime[16] = "", vdepth[16] = "", varrows[8] = "";
    char vmaxfps[16] = "", vsound[8] = "";
    while (fgets(line, sizeof line, f)) {
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line, *val = eq + 1;
        trim(key); trim(val);
        if (!*key || !*val) continue;

        /* Engine paths are case-sensitive: keep the value verbatim. */
        char rawval[512];
        snprintf(rawval, sizeof rawval, "%s", val);

        lower_in_place(key);
        lower_in_place(val);
        if (strcmp(key, "board") == 0) snprintf(vboard, sizeof vboard, "%s", val);
        else if (strcmp(key, "pieces") == 0) snprintf(vpieces, sizeof vpieces, "%s", val);
        else if (strcmp(key, "animation") == 0) snprintf(vanim, sizeof vanim, "%s", val);
        else if (strcmp(key, "engine") == 0) snprintf(vengine, sizeof vengine, "%s", rawval);
        else if (strcmp(key, "board_size") == 0) snprintf(vboardsize, sizeof vboardsize, "%s", val);
        else if (strcmp(key, "engine_threads") == 0) snprintf(vthreads, sizeof vthreads, "%s", val);
        else if (strcmp(key, "engine_hash") == 0) snprintf(vhash, sizeof vhash, "%s", val);
        else if (strcmp(key, "engine_multipv") == 0) snprintf(vmultipv, sizeof vmultipv, "%s", val);
        else if (strcmp(key, "engine_time") == 0) snprintf(vtime, sizeof vtime, "%s", val);
        else if (strcmp(key, "engine_depth") == 0) snprintf(vdepth, sizeof vdepth, "%s", val);
        else if (strcmp(key, "engine_arrows") == 0) snprintf(varrows, sizeof varrows, "%s", val);
        else if (strcmp(key, "max_fps") == 0) snprintf(vmaxfps, sizeof vmaxfps, "%s", val);
        else if (strcmp(key, "sound") == 0) snprintf(vsound, sizeof vsound, "%s", val);
    }
    fclose(f);

    gui_apply_style_names(g,
                          vboard[0] ? vboard : NULL,
                          vpieces[0] ? vpieces : NULL,
                          vanim[0] ? vanim : NULL);

    if (vengine[0]) {
        const char *ep = ai_find_engine(vengine);
        if (ep) snprintf(g->engine_path, sizeof g->engine_path, "%s", ep);
    }

    if (vboardsize[0]) {
        int sz = atoi(vboardsize);
        if (sz > 0) {
            g->pref_zoom = clamp_zoom((float)sz / (float)SQ_SIZE);
            g->zoom = g->pref_zoom;
        }
    }

    if (vthreads[0]) { int n = atoi(vthreads); if (n > 0) g->eng_threads = n; }
    if (vhash[0])    { int n = atoi(vhash);    if (n > 0) g->eng_hash = n; }
    if (vmultipv[0]) { int n = atoi(vmultipv); if (n >= 0 && n <= AI_MAX_LINES) g->eng_multipv = n; }
    if (vtime[0])    { int n = atoi(vtime);    if (n >= 0) g->eng_time_ms = n; }
    if (vdepth[0])   { int n = atoi(vdepth);   if (n >= 0) g->eng_depth = n; }
    if (varrows[0])  g->engine_arrows = (atoi(varrows) != 0);
    if (vmaxfps[0])  { int n = atoi(vmaxfps); if (n >= 0) g->max_fps = n; }
    if (vsound[0])   g->sound = (atoi(vsound) != 0);
}

/* Persist the current look to chess.conf (only when it changed). */
void gui_save_config(Gui *g)
{
    if (!g || !g->config_dirty) return;
    const char *path = g->config_path[0] ? g->config_path : "chess.conf";

    path_make_parent(path);
    FILE *f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "# OpenChess configuration (auto-saved)\n");
    if (g->boards.items && g->board_index >= 0 && g->board_index < g->boards.count)
        fprintf(f, "board = %s\n", g->boards.items[g->board_index].key);
    if (g->pieces.items && g->piece_index >= 0 && g->piece_index < g->pieces.count)
        fprintf(f, "pieces = %s\n", g->pieces.items[g->piece_index].key);
    fprintf(f, "animation = %s\n", ANIM_KEYS[g->anim_style]);
    fprintf(f, "board_size = %d\n", (int)lroundf(SQ_SIZE * g->pref_zoom));
    fprintf(f, "engine_multipv = %d\n", g->eng_multipv);
    fprintf(f, "engine_threads = %d\n", g->eng_threads);
    fprintf(f, "engine_hash = %d\n", g->eng_hash);
    fprintf(f, "engine_time = %d\n", g->eng_time_ms);
    fprintf(f, "engine_depth = %d\n", g->eng_depth);
    fprintf(f, "engine_arrows = %d\n", g->engine_arrows ? 1 : 0);
    fprintf(f, "sound = %d\n", g->sound ? 1 : 0);
    fprintf(f, "max_fps = %d\n", g->max_fps);
    if (g->engine_path[0]) fprintf(f, "engine = %s\n", g->engine_path);
    fclose(f);
}

static void flash_style(Gui *g, const char *what, const char *label)
{
    char buf[96];
    snprintf(buf, sizeof buf, "%s: %s", what, label);
    set_msg(g, buf, NULL);
}

static const char *theme_label(const ThemeList *list, int index)
{
    if (!list || index < 0 || index >= list->count) return "?";
    return list->items[index].label;
}

void gui_cycle_board(Gui *g)
{
    if (g->boards.count > 0) {
        g->board_index = (g->board_index + 1) % g->boards.count;
        load_board_style(g);
        g->config_dirty = true;
        flash_style(g, "Board", theme_label(&g->boards, g->board_index));
    }
}

void gui_cycle_pieces(Gui *g)
{
    if (g->pieces.count > 0) {
        g->piece_index = (g->piece_index + 1) % g->pieces.count;
        load_piece_style(g);
        g->config_dirty = true;
        flash_style(g, "Pieces", theme_label(&g->pieces, g->piece_index));
    }
}

void gui_cycle_anim(Gui *g)
{
    g->anim_style = (AnimStyle)((g->anim_style + 1) % ANIM_STYLE_COUNT);
    clear_anims(g);
    g->config_dirty = true;
    flash_style(g, "Anim", ANIM_LABELS[g->anim_style]);
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */

Gui *gui_create(void)
{
    Gui *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    g->ui_scale = 1.0f;
    g->zoom = 1.0f;
    g->pref_zoom = 1.0f;
    g->debug_ui = (getenv("OPENCHESS_DEBUG_UI") != NULL);
    g->max_fps = 0;             /* 0 = vsync */
    g->sound = true;
    g->pan_x = g->pan_y = 0.0f;
    g->tf_scale = 1.0f;
    g->tf_ux = g->tf_uy = 1.0f;
    layout_base(g);
    g->scene = SCENE_MENU;
    g->mode = MODE_ANALYSIS;
    g->menu_index = 1;          /* Analysis is the default selection */
    g->selected = -1;
    g->promo_from = g->promo_to = -1;
    g->input.active = false;
    g->san_open = false;
    g->ann_from = g->ann_to = -1;
    g->eng_multipv = 1;
    g->eng_threads = 1;
    g->eng_hash = 16;
    g->eng_time_ms = 0;
    g->eng_depth = 0;
    g->engine_arrows = true;
    g->eng_ctrl_focus = -1;
    themes_load(&g->boards, &g->pieces, path_assets());
    g->board_index = themes_index_of(&g->boards, "icy_sea");
    if (g->board_index < 0) g->board_index = 0;
    g->piece_index = themes_index_of(&g->pieces, "cases");
    if (g->piece_index < 0) g->piece_index = 0;
    g->anim_style  = ANIM_ARCADE;
    g->auto_flip = false;
    g->setup_field = 0;
    g->setup_side = 0;
    g->setup_level = 1;         /* Medium */
    snprintf(g->white_name, sizeof g->white_name, "White");
    snprintf(g->black_name, sizeof g->black_name, "Black");
    g->hj_focus = 0;
    snprintf(g->config_path, sizeof g->config_path, "%s", path_config());
    snprintf(g->net_addr, sizeof g->net_addr, "127.0.0.1");
    snprintf(g->net_port, sizeof g->net_port, "7777");
    const char *ep = ai_find_engine(NULL);
    if (ep) snprintf(g->engine_path, sizeof g->engine_path, "%s", ep);
    board_reset(&g->board);
    recompute_state(g);
    fen_refresh(g);
    return g;
}

void gui_init_assets(Gui *g, SDL_Renderer *ren)
{
    g->ren = ren;
    g->win = SDL_RenderGetWindow(ren);

    /* Size the window to the magnified base canvas and allow zooming out to 0.5. */
    if (g->win) {
        SDL_SetWindowMinimumSize(g->win,
                                 (int)lroundf(g->win_w * 0.5f),
                                 (int)lroundf(g->win_h * 0.5f));
        SDL_SetWindowSize(g->win,
                          (int)lroundf(g->win_w * g->zoom),
                          (int)lroundf(g->win_h * g->zoom));
    }

    /* Display density = device pixels per window point (2 on Retina). The whole
     * UI is drawn on the fixed base canvas and magnified by density * zoom. */
    int ww = g->win_w, wh = g->win_h, ow = g->win_w, oh = g->win_h;
    if (g->win) SDL_GetWindowSize(g->win, &ww, &wh);
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float density = (ww > 0) ? (float)ow / (float)ww : 1.0f;
    if (density < 1.0f) density = 1.0f;
    g->ui_scale = density;

    apply_render_scale(g);
    rebuild_fonts(g);
    load_piece_style(g);       /* ensure the initial piece textures exist */
    load_board_style(g);
}

static void destroy_thumbs(SDL_Texture ***arr, int n)
{
    if (!arr || !*arr) return;
    for (int i = 0; i < n; i++)
        if ((*arr)[i]) SDL_DestroyTexture((*arr)[i]);
    free(*arr);
    *arr = NULL;
}

void gui_destroy(Gui *g)
{
    if (!g) return;
    if (g->ai) ai_stop(g->ai);
    if (g->eval_ai) ai_stop(g->eval_ai);
    if (g->net) {
        if (net_state(g->net) == NET_STATE_CONNECTED) net_send(g->net, "BYE");
        net_close(g->net);
    }
    destroy_piece_textures(g);
    if (g->tex_board) SDL_DestroyTexture(g->tex_board);
    destroy_thumbs(&g->board_thumbs, g->boards.count);
    destroy_thumbs(&g->piece_thumbs, g->pieces.count);
    themes_free(&g->boards);
    themes_free(&g->pieces);
    if (g->font_piece[0]) TTF_CloseFont(g->font_piece[0]);
    if (g->font_piece[1]) TTF_CloseFont(g->font_piece[1]);
    if (g->font_ui) TTF_CloseFont(g->font_ui);
    if (g->font_small) TTF_CloseFont(g->font_small);
    if (g->font_tiny) TTF_CloseFont(g->font_tiny);
    free(g);
}

bool gui_quit(Gui *g)
{
    return g->quit;
}

void gui_anim_advance(Gui *g, Uint32 now)
{
    while (g->anim_count > 0) {
        AnimStep *s = &g->anim_queue[0];
        if (s->dur == 0) {
            memmove(&g->anim_queue[0], &g->anim_queue[1],
                    sizeof(AnimStep) * (g->anim_count - 1));
            g->anim_count--;
            continue;
        }
        if (!s->started) { s->started = true; s->start = now; }
        if ((Uint32)(now - s->start) < s->dur) break;
        memmove(&g->anim_queue[0], &g->anim_queue[1],
                sizeof(AnimStep) * (g->anim_count - 1));
        g->anim_count--;
    }
}

static bool san_char_ok(char c)
{
    if (c >= 'a' && c <= 'h') return true;
    if (c >= '1' && c <= '8') return true;
    switch (c) {
        case 'N': case 'B': case 'R': case 'Q': case 'K':
        case 'O': case 'x': case '=': case '+': case '#':
        case '-': case '.': case ' ': case '0':
            return true;
        default:
            return false;
    }
}

static bool fen_char_ok(char c)
{
    return c >= 0x20 && c < 0x7f;   /* any printable ASCII */
}

/* ---- welcome / mode menu ---- */

static void menu_move(Gui *g, int dir)
{
    int count = menu_count(g);
    for (int n = 0; n < count; n++) {
        g->menu_index = (g->menu_index + dir + count) % count;
        if (menu_item_enabled(g, g->menu_index)) return;
    }
}

static void menu_activate(Gui *g)
{
    if (g->saved.valid && g->menu_index == 0) { resume_game(g); return; }

    switch (menu_base_index(g, g->menu_index)) {
        case 0:
            g->setup_field = 0;
            g->menu_msg[0] = 0;
            g->scene = SCENE_SINGLE_SETUP;
            break;
        case 1: start_analysis(g); break;
        case 2:
            if (!net_available()) {
                snprintf(g->menu_msg, sizeof g->menu_msg,
                         "Networking unavailable (install SDL2_net)");
                break;
            }
            g->hj_focus = 0;
            g->net_waiting = false;
            g->menu_msg[0] = 0;
            if (!g->net_addr[0]) snprintf(g->net_addr, sizeof g->net_addr, "127.0.0.1");
            if (!g->net_port[0]) snprintf(g->net_port, sizeof g->net_port, "7777");
            g->scene = SCENE_HOSTJOIN;
            break;
        case 3: open_appearance(g, SCENE_MENU); break;
        case 4: open_settings_scene(g, SCENE_MENU); break;
        case 5: g->quit = true; break;
        default:
            snprintf(g->menu_msg, sizeof g->menu_msg,
                     "'%s' is not available yet", menu_label(g, g->menu_index));
            break;
    }
}

static void handle_menu_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_UP || k == SDLK_w) { menu_move(g, -1); return; }
    if (k == SDLK_DOWN || k == SDLK_s) { menu_move(g, 1); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) menu_activate(g);
}

static void handle_menu_mousedown(Gui *g)
{
    for (int i = 0; i < menu_count(g); i++) {
        SDL_Rect r;
        menu_item_rect(g, i, &r);
        if (!pt_in(&r, g->mouse.x, g->mouse.y)) continue;
        g->menu_index = i;
        if (menu_item_enabled(g, i)) {
            menu_activate(g);
        } else {
            snprintf(g->menu_msg, sizeof g->menu_msg,
                     "'%s' is not available yet", menu_label(g, i));
        }
        return;
    }
}

static void handle_menu_mousemotion(Gui *g)
{
    for (int i = 0; i < menu_count(g); i++) {
        SDL_Rect r;
        menu_item_rect(g, i, &r);
        if (pt_in(&r, g->mouse.x, g->mouse.y) && menu_item_enabled(g, i)) {
            g->menu_index = i;
            return;
        }
    }
}

/* ---- singleplayer setup ---- */

static void setup_rects(const Gui *g, SDL_Rect sides[2], SDL_Rect levels[3],
                        SDL_Rect *start, SDL_Rect *back)
{
    int bw = 120, bh = 48, gap = 20;
    int x0 = (g->win_w - (2 * bw + gap)) / 2;
    sides[0] = (SDL_Rect){ x0, 300, bw, bh };
    sides[1] = (SDL_Rect){ x0 + bw + gap, 300, bw, bh };

    int lg = 16;
    int lx = (g->win_w - (3 * bw + 2 * lg)) / 2;
    for (int i = 0; i < 3; i++)
        levels[i] = (SDL_Rect){ lx + i * (bw + lg), 420, bw, bh };

    start->w = 150; start->h = 46; start->x = g->win_w / 2 - 160; start->y = 540;
    back->w  = 150; back->h  = 46; back->x  = g->win_w / 2 + 10;  back->y  = 540;
}

static void setup_change(Gui *g, int dir)
{
    if (g->setup_field == 0)
        g->setup_side = (g->setup_side + dir + 2) % 2;
    else
        g->setup_level = (g->setup_level + dir + AI_LEVEL_COUNT) % AI_LEVEL_COUNT;
}

static void setup_begin(Gui *g)
{
    if (!g->engine_path[0]) {
        snprintf(g->menu_msg, sizeof g->menu_msg, "Stockfish not found - run 'brew install stockfish'");
        return;
    }
    start_single(g);
}

static void handle_setup_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE) { g->scene = SCENE_MENU; return; }
    if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_TAB) { g->setup_field ^= 1; return; }
    if (k == SDLK_LEFT) { setup_change(g, -1); return; }
    if (k == SDLK_RIGHT) { setup_change(g, 1); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) setup_begin(g);
}

static void handle_setup_mousedown(Gui *g)
{
    SDL_Rect sides[2], levels[3], start, back;
    setup_rects(g, sides, levels, &start, &back);
    SDL_Point p = g->mouse;

    for (int i = 0; i < 2; i++)
        if (pt_in(&sides[i], p.x, p.y)) { g->setup_field = 0; g->setup_side = i; return; }
    for (int i = 0; i < 3; i++)
        if (pt_in(&levels[i], p.x, p.y)) { g->setup_field = 1; g->setup_level = i; return; }
    if (pt_in(&start, p.x, p.y)) { setup_begin(g); return; }
    if (pt_in(&back, p.x, p.y)) { g->scene = SCENE_MENU; return; }
}

/* ---- local multiplayer host/join ---- */

static void hj_rects(const Gui *g, SDL_Rect *addr, SDL_Rect *port, SDL_Rect *host,
                     SDL_Rect *join, SDL_Rect *back)
{
    int fx = g->win_w / 2 - 200;
    addr->x = fx; addr->y = 290; addr->w = 400; addr->h = 44;
    port->x = fx; port->y = 360; port->w = 200; port->h = 44;
    host->x = g->win_w / 2 - 250; host->y = 470; host->w = 150; host->h = 48;
    join->x = g->win_w / 2 - 75;  join->y = 470; join->w = 150; join->h = 48;
    back->x = g->win_w / 2 + 100; back->y = 470; back->w = 150; back->h = 48;
}

static unsigned short hj_port(Gui *g)
{
    int p = atoi(g->net_port);
    if (p <= 0 || p > 65535) p = 7777;
    return (unsigned short)p;
}

static void hj_begin_host(Gui *g)
{
    if (g->net) { net_close(g->net); g->net = NULL; }
    g->net = net_host(hj_port(g));
    if (!g->net) {
        snprintf(g->menu_msg, sizeof g->menu_msg,
                 "Could not listen on port %s", g->net_port);
        return;
    }
    g->net_waiting = true;
    snprintf(g->menu_msg, sizeof g->menu_msg,
             "Waiting for opponent on port %s ...", g->net_port);
}

static void hj_begin_join(Gui *g)
{
    if (g->net) { net_close(g->net); g->net = NULL; }
    g->net = net_join(g->net_addr, hj_port(g));
    if (!g->net) {
        snprintf(g->menu_msg, sizeof g->menu_msg,
                 "Could not connect to %s:%s", g->net_addr, g->net_port);
        return;
    }
    enter_local_game(g, net_role(g->net) == NET_ROLE_JOIN ? BLACK : WHITE);
}

static void handle_hostjoin_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE) { go_to_menu(g); return; }
    if (k == SDLK_TAB || k == SDLK_DOWN) { g->hj_focus = (g->hj_focus + 1) % 5; return; }
    if (k == SDLK_UP) { g->hj_focus = (g->hj_focus + 4) % 5; return; }

    if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
        char *dst = (g->hj_focus == 0) ? g->net_addr
                  : (g->hj_focus == 1) ? g->net_port : NULL;
        if (dst) { size_t n = strlen(dst); if (n) dst[n - 1] = '\0'; }
        return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
        if (g->hj_focus == 2) hj_begin_host(g);
        else if (g->hj_focus == 3) hj_begin_join(g);
        else if (g->hj_focus == 4) go_to_menu(g);
        else g->hj_focus = (g->hj_focus + 1) % 5;
    }
}

static void handle_hostjoin_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    char *dst = (g->hj_focus == 0) ? g->net_addr
              : (g->hj_focus == 1) ? g->net_port : NULL;
    if (!dst) return;
    size_t cap = (g->hj_focus == 0) ? sizeof g->net_addr : sizeof g->net_port;
    size_t len = strlen(dst);
    for (const char *p = te->text; *p; p++) {
        if (len + 1 >= cap) break;
        char c = *p;
        bool ok = (g->hj_focus == 0)
                ? (isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_')
                : (c >= '0' && c <= '9');
        if (ok) dst[len++] = c;
    }
    dst[len] = '\0';
}

static void handle_hostjoin_mousedown(Gui *g)
{
    SDL_Rect addr, port, host, join, back;
    hj_rects(g, &addr, &port, &host, &join, &back);
    SDL_Point p = g->mouse;

    if (pt_in(&addr, p.x, p.y)) { g->hj_focus = 0; return; }
    if (pt_in(&port, p.x, p.y)) { g->hj_focus = 1; return; }
    if (pt_in(&host, p.x, p.y)) { g->hj_focus = 2; hj_begin_host(g); return; }
    if (pt_in(&join, p.x, p.y)) { g->hj_focus = 3; hj_begin_join(g); return; }
    if (pt_in(&back, p.x, p.y)) { g->hj_focus = 4; go_to_menu(g); return; }
}

/* ---- appearance picker ---- */

#define APE_COLS 6
#define APE_ROWS 3
#define APE_PER_PAGE (APE_COLS * APE_ROWS)

static void ape_tab_rect(const Gui *g, int i, SDL_Rect *r)
{
    int tw = 170, th = 40, gap = 20;
    int x0 = (g->win_w - (3 * tw + 2 * gap)) / 2;
    r->x = x0 + i * (tw + gap);
    r->y = 96;
    r->w = tw;
    r->h = th;
}

static void ape_cell_rect(const Gui *g, int j, SDL_Rect *r)
{
    int cw = 170, ch = 150, gap = 12;
    int x0 = (g->win_w - (APE_COLS * cw + (APE_COLS - 1) * gap)) / 2;
    r->x = x0 + (j % APE_COLS) * (cw + gap);
    r->y = 170 + (j / APE_COLS) * (ch + gap);
    r->w = cw;
    r->h = ch;
}

static void ape_back_rect(const Gui *g, SDL_Rect *r)
{
    r->w = 140; r->h = 42;
    r->x = g->win_w - 180; r->y = g->win_h - 70;
}

static int appearance_count(Gui *g, int tab)
{
    if (tab == 0) return g->boards.count;
    if (tab == 1) return g->pieces.count;
    return ANIM_STYLE_COUNT;
}

static void open_appearance(Gui *g, Scene return_to)
{
    g->ape_return_scene = return_to;
    g->ape_tab = 0;
    g->ape_sel[0] = g->board_index;
    g->ape_sel[1] = g->piece_index;
    g->ape_sel[2] = (int)g->anim_style;
    g->ape_started = SDL_GetTicks();
    g->scene = SCENE_APPEARANCE;
}

static void appearance_apply(Gui *g)
{
    switch (g->ape_tab) {
        case 0: g->board_index = g->ape_sel[0]; load_board_style(g); break;
        case 1: g->piece_index = g->ape_sel[1]; load_piece_style(g); break;
        default: g->anim_style = (AnimStyle)g->ape_sel[2]; clear_anims(g); break;
    }
    g->config_dirty = true;
}

static void appearance_move(Gui *g, int delta)
{
    int n = appearance_count(g, g->ape_tab);
    if (n <= 0) return;
    int sel = g->ape_sel[g->ape_tab] + delta;
    if (sel < 0) sel = 0;
    if (sel >= n) sel = n - 1;
    g->ape_sel[g->ape_tab] = sel;
    appearance_apply(g);
}

static void handle_appearance_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { g->scene = g->ape_return_scene; return; }
    if (k == SDLK_LEFT)  { appearance_move(g, -1); return; }
    if (k == SDLK_RIGHT) { appearance_move(g, 1); return; }
    if (k == SDLK_UP)    { appearance_move(g, -APE_COLS); return; }
    if (k == SDLK_DOWN)  { appearance_move(g, APE_COLS); return; }
    if (k == SDLK_TAB)   { g->ape_tab = (g->ape_tab + 1) % 3; return; }
    if (k == SDLK_1 || k == SDLK_2 || k == SDLK_3) {
        g->ape_tab = (int)(k - SDLK_1);
        return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
        appearance_apply(g);
        return;
    }
}

static void handle_appearance_mousedown(Gui *g)
{
    SDL_Point p = g->mouse;

    for (int i = 0; i < 3; i++) {
        SDL_Rect r;
        ape_tab_rect(g, i, &r);
        if (pt_in(&r, p.x, p.y)) { g->ape_tab = i; return; }
    }
    SDL_Rect back;
    ape_back_rect(g, &back);
    if (pt_in(&back, p.x, p.y)) { g->scene = g->ape_return_scene; return; }

    int tab = g->ape_tab;
    int n = appearance_count(g, tab);
    int start = (g->ape_sel[tab] / APE_PER_PAGE) * APE_PER_PAGE;
    for (int j = 0; j < APE_PER_PAGE; j++) {
        int idx = start + j;
        if (idx >= n) break;
        SDL_Rect c;
        ape_cell_rect(g, j, &c);
        if (pt_in(&c, p.x, p.y)) {
            g->ape_sel[tab] = idx;
            appearance_apply(g);
            return;
        }
    }
}

/* ---- engine selection ---- */

#define ENGINE_MAX 8

static void detect_engines(Gui *g)
{
    g->engine_count = 0;

    static const char *names[] = {
        "stockfish", "lc0", "komodo", "ethereal", "berserk", "arasan",
        "crafty", "fruit", "glaurung", "houdini", "rubichess", "fire",
        NULL
    };

    const char *path = getenv("PATH");
    if (path) {
        char *dup = strdup(path);
        if (dup) {
            for (char *dir = strtok(dup, ":"); dir && g->engine_count < ENGINE_MAX;
                 dir = strtok(NULL, ":")) {
                for (int i = 0; names[i] && g->engine_count < ENGINE_MAX; i++) {
                    char full[512];
                    snprintf(full, sizeof full, "%s/%s", dir, names[i]);
                    if (access(full, X_OK) != 0) continue;
                    bool seen = false;
                    for (int j = 0; j < g->engine_count; j++)
                        if (strcmp(g->engine_candidates[j], full) == 0) seen = true;
                    if (!seen)
                        snprintf(g->engine_candidates[g->engine_count++],
                                 sizeof g->engine_candidates[0], "%s", full);
                }
            }
            free(dup);
        }
    }

    if (g->engine_path[0] && g->engine_count < ENGINE_MAX) {
        bool seen = false;
        for (int i = 0; i < g->engine_count; i++)
            if (strcmp(g->engine_candidates[i], g->engine_path) == 0) seen = true;
        if (!seen)
            snprintf(g->engine_candidates[g->engine_count++],
                     sizeof g->engine_candidates[0], "%s", g->engine_path);
    }
}

static void apply_engine(Gui *g, const char *path)
{
    if (!path || !*path) return;

    if (g->eval_ai) { ai_stop(g->eval_ai); g->eval_ai = NULL; }
    if (g->ai) { ai_stop(g->ai); g->ai = NULL; }

    snprintf(g->engine_path, sizeof g->engine_path, "%s", path);
    g->config_dirty = true;

    AiEngine *probe = ai_start(g->engine_path);
    if (!probe) {
        snprintf(g->engine_status, sizeof g->engine_status,
                 "Failed to start: %s", path);
        return;
    }
    ai_stop(probe);
    snprintf(g->engine_status, sizeof g->engine_status, "Engine: %s", path);

    if (g->mode == MODE_ANALYSIS && g->scene == SCENE_GAME) eval_restart(g);
}

static void open_settings_scene(Gui *g, Scene return_to)
{
    detect_engines(g);
    g->engine_sel = 0;
    for (int i = 0; i < g->engine_count; i++)
        if (strcmp(g->engine_candidates[i], g->engine_path) == 0) g->engine_sel = i;
    g->engine_custom_focus = (g->engine_count == 0);
    g->eng_ctrl_focus = -1;
    snprintf(g->engine_custom, sizeof g->engine_custom, "%s", g->engine_path);
    g->engine_status[0] = 0;
    g->engine_return_scene = return_to;
    g->settings_tab = 0;
    g->settings_row = 0;
    g->scene = SCENE_SETTINGS;
}

/* Engine screen: the engine list is on the right, the analysis controls on
 * the left. Row order matches ENG_CTRL_LABELS. */
#define ENG_CTRL_COUNT 5
static const char *ENG_CTRL_LABELS[ENG_CTRL_COUNT] = {
    "Lines", "Threads", "Hash (MB)", "Time (ms)", "Depth"
};

static void engine_rects(const Gui *g, SDL_Rect items[ENGINE_MAX],
                         SDL_Rect *custom, SDL_Rect *back)
{
    int x = 620, y0 = 170, w = g->win_w - 700, h = 40, gap = 8;
    if (w < 200) { x = 80; w = g->win_w - 160; }
    for (int i = 0; i < ENGINE_MAX; i++)
        items[i] = (SDL_Rect){ x, y0 + i * (h + gap), w, h };
    custom->x = 80; custom->y = g->win_h - 150; custom->w = g->win_w - 260; custom->h = 40;
    back->w = 140; back->h = 42; back->x = g->win_w - 160; back->y = g->win_h - 90;
}

static void engine_ctrl_rects(const Gui *g,
                              SDL_Rect minus[ENG_CTRL_COUNT],
                              SDL_Rect plus[ENG_CTRL_COUNT])
{
    (void)g;
    int x = 80, y0 = 170, w = 30, h = 30, gap = 8;
    for (int i = 0; i < ENG_CTRL_COUNT; i++) {
        int y = y0 + i * (h + gap);
        minus[i] = (SDL_Rect){ x + 224, y, w, h };
        plus[i]  = (SDL_Rect){ x + 260, y, w, h };
    }
}

static int eng_ctrl_value(const Gui *g, int i)
{
    switch (i) {
        case 0: return g->eng_multipv;
        case 1: return g->eng_threads;
        case 2: return g->eng_hash;
        case 3: return g->eng_time_ms;
        default: return g->eng_depth;
    }
}

static void eng_ctrl_adjust(Gui *g, int i, int dir)
{
    switch (i) {
        case 0: g->eng_multipv = clampi(g->eng_multipv + dir, 0, AI_MAX_LINES); break;
        case 1: g->eng_threads = clampi(g->eng_threads + dir, 1, 256); break;
        case 2: g->eng_hash    = clampi(g->eng_hash + dir * 16, 16, 4096); break;
        case 3: g->eng_time_ms = clampi(g->eng_time_ms + dir * 100, 0, 60000); break;
        default:g->eng_depth   = clampi(g->eng_depth + dir, 0, 60); break;
    }
    g->config_dirty = true;

    if (g->eval_ai) {
        if (i == 1) ai_set_threads(g->eval_ai, g->eng_threads);
        else if (i == 2) ai_set_hash(g->eval_ai, g->eng_hash);
    }
    if (g->ai) {
        if (i == 3) ai_set_movetime(g->ai, g->eng_time_ms > 0 ? g->eng_time_ms : 1);
        else if (i == 4) ai_set_depth(g->ai, g->eng_depth);
        else if (i == 1) ai_set_threads(g->ai, g->eng_threads);
        else if (i == 2) ai_set_hash(g->ai, g->eng_hash);
    }
    if (i == 0 || i == 3 || i == 4)
        if (g->mode == MODE_ANALYSIS && g->scene == SCENE_GAME) eval_restart(g);
}

static void handle_engine_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;

    if (g->engine_custom_focus) {
        if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            size_t n = strlen(g->engine_custom);
            if (n) g->engine_custom[n - 1] = '\0';
            return;
        }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) apply_engine(g, g->engine_custom);
        return;
    }

    if (g->eng_ctrl_focus >= 0) {
        if (k == SDLK_LEFT)  { eng_ctrl_adjust(g, g->eng_ctrl_focus, -1); return; }
        if (k == SDLK_RIGHT) { eng_ctrl_adjust(g, g->eng_ctrl_focus, +1); return; }
        if (k == SDLK_UP)   { if (g->eng_ctrl_focus > 0) g->eng_ctrl_focus--; return; }
        if (k == SDLK_DOWN) {
            if (g->eng_ctrl_focus < ENG_CTRL_COUNT - 1) g->eng_ctrl_focus++;
            return;
        }
    }

    if (k == SDLK_UP) { if (g->engine_sel > 0) g->engine_sel--; return; }
    if (k == SDLK_DOWN) {
        if (g->engine_sel + 1 < g->engine_count) g->engine_sel++;
        return;
    }
    if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && g->engine_count > 0)
        apply_engine(g, g->engine_candidates[g->engine_sel]);
}

static void handle_engine_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    if (!g->engine_custom_focus) return;
    size_t n = strlen(g->engine_custom);
    for (const char *p = te->text; *p; p++) {
        if (n + 1 >= sizeof g->engine_custom) break;
        if (*p >= 0x20 && *p < 0x7f) g->engine_custom[n++] = *p;
    }
    g->engine_custom[n] = '\0';
}

static void handle_engine_mousedown(Gui *g)
{
    SDL_Rect items[ENGINE_MAX], custom, back;
    SDL_Rect minus[ENG_CTRL_COUNT], plus[ENG_CTRL_COUNT];
    engine_rects(g, items, &custom, &back);
    engine_ctrl_rects(g, minus, plus);
    SDL_Point p = g->mouse;

    for (int i = 0; i < ENG_CTRL_COUNT; i++) {
        if (pt_in(&minus[i], p.x, p.y) || pt_in(&plus[i], p.x, p.y)) {
            g->eng_ctrl_focus = i;
            g->engine_custom_focus = false;
            eng_ctrl_adjust(g, i, pt_in(&minus[i], p.x, p.y) ? -1 : +1);
            return;
        }
    }

    for (int i = 0; i < g->engine_count; i++)
        if (pt_in(&items[i], p.x, p.y)) {
            g->engine_sel = i;
            g->engine_custom_focus = false;
            g->eng_ctrl_focus = -1;
            apply_engine(g, g->engine_candidates[i]);
            return;
        }
    if (pt_in(&custom, p.x, p.y)) {
        g->engine_custom_focus = true;
        g->eng_ctrl_focus = -1;
        return;
    }
    if (pt_in(&back, p.x, p.y)) { g->scene = g->engine_return_scene; return; }
}

/* ---- game ---- */

static void copy_fen_to_clipboard(Gui *g)
{
    char fen[FEN_MAX];
    fen_generate(&g->board, fen, sizeof fen);
    if (SDL_SetClipboardText(fen) == 0) set_msg(g, "FEN copied", NULL);
    else set_msg(g, "Clipboard unavailable", NULL);
}

/* ---- PGN export ---- */

static void pgn_open_prompt(Gui *g)
{
    if (g->ply <= 0) { set_msg(g, "No moves to export", NULL); return; }
    san_close_box(g);
    g->fen_active = false;
    g->pgn_prompt = true;
    g->pgn_len = 0;
    g->pgn_name[0] = '\0';
}

static void pgn_save(Gui *g)
{
    if (g->pgn_len == 0) return;

    char path[1200];
    path_game_file(path, sizeof path, g->pgn_name);
    path_make_parent(path);

    FILE *f = fopen(path, "w");
    if (!f) { set_msg(g, "Could not write PGN", NULL); return; }

    char date[16] = "????.??.??";
    time_t t = time(NULL);
    struct tm *tmv = localtime(&t);
    if (tmv)
        snprintf(date, sizeof date, "%04d.%02d.%02d",
                 tmv->tm_year + 1900, tmv->tm_mon + 1, tmv->tm_mday);

    const char *result = pgn_result(&g->board, g->state);
    pgn_write(f, (const char (*)[8])g->move_san, g->ply,
              "OpenChess", "OpenChess", date, 1,
              g->white_name, g->black_name, result);
    fclose(f);

    g->pgn_prompt = false;
    set_msg(g, "Saved PGN: %s", path);
}

static void handle_game_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (g->pgn_prompt) {
        if (k == SDLK_ESCAPE) { g->pgn_prompt = false; return; }
        if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            if (g->pgn_len > 0) g->pgn_name[--g->pgn_len] = '\0';
            return;
        }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) pgn_save(g);
        return;
    }

    if (k == SDLK_ESCAPE) {
        if (g->fen_active) { g->fen_active = false; fen_refresh(g); return; }
        if (g->san_open) { san_close_box(g); return; }
        clear_selection(g);
        ann_clear(g);
        g->promo_from = g->promo_to = -1;
        return;
    }

    if (ctrl && k == SDLK_u) {
        if (g->mode == MODE_LOCAL) set_msg(g, "Undo disabled in multiplayer", NULL);
        else do_undo(g);
        return;
    }
    if (ctrl && k == SDLK_r) {
        if (g->mode == MODE_LOCAL) set_msg(g, "Restart disabled in multiplayer", NULL);
        else do_restart(g);
        return;
    }
    if (ctrl && k == SDLK_b) { gui_cycle_board(g); return; }
    if (ctrl && k == SDLK_p) { gui_cycle_pieces(g); return; }
    if (ctrl && k == SDLK_m) { gui_cycle_anim(g); return; }
    if (ctrl && k == SDLK_c) { copy_fen_to_clipboard(g); return; }
    if (ctrl && k == SDLK_e) { open_settings_scene(g, SCENE_GAME); return; }
    if (ctrl && k == SDLK_s) { pgn_open_prompt(g); return; }
    if (ctrl && k == SDLK_f) {
        g->flipped = !g->flipped;
        g->auto_flip = false;
        set_msg(g, g->flipped ? "Board flipped" : "Board normal", NULL);
        return;
    }

    if (g->fen_active) {
        if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            if (g->fen_len > 0) g->fen_text[--g->fen_len] = 0;
            return;
        }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            if (load_fen(g, g->fen_text)) {
                g->fen_active = false;
                fen_refresh(g);
                set_msg(g, "Position loaded", NULL);
            } else {
                set_msg(g, "Invalid FEN", NULL);
            }
            return;
        }
        return;
    }

    if (g->san_open) {
        if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            if (g->input.len > 0) g->input.text[--g->input.len] = 0;
            return;
        }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            if (apply_text_san(g)) san_close_box(g);
            return;
        }
        return;
    }

    /* Enter reveals the SAN box; Enter again submits. */
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        san_open_box(g);
        return;
    }
}

static void handle_game_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    if (g->pgn_prompt) {
        for (const char *p = te->text; *p; p++) {
            if (g->pgn_len >= (int)sizeof g->pgn_name - 1) break;
            if (*p >= 0x20 && *p < 0x7f) g->pgn_name[g->pgn_len++] = *p;
        }
        g->pgn_name[g->pgn_len] = '\0';
        return;
    }

    if (g->fen_active) {
        for (const char *p = te->text; *p; p++) {
            if (g->fen_len >= FEN_MAX - 1) break;
            if (fen_char_ok(*p)) g->fen_text[g->fen_len++] = *p;
        }
        g->fen_text[g->fen_len] = 0;
        return;
    }

    if (!g->san_open) return;
    for (const char *p = te->text; *p; p++) {
        if (g->input.len >= (int)sizeof g->input.text - 1) break;
        if (san_char_ok(*p)) g->input.text[g->input.len++] = *p;
    }
    g->input.text[g->input.len] = 0;
}

static void handle_game_mousedown(Gui *g)
{
    SDL_Point p = g->mouse;
    g->dragging = false;

    if (g->pgn_prompt) return;   /* modal: keyboard only */

    /* MultiPV slider + engine-arrow toggle (analysis). */
    if (g->mode == MODE_ANALYSIS) {
        SDL_Rect box;
        engine_arrows_rect(g, &box);
        SDL_Rect ahit = { box.x - 4, box.y - 4, box.w + 60, box.h + 8 };
        if (pt_in(&ahit, p.x, p.y)) {
            g->engine_arrows = !g->engine_arrows;
            g->config_dirty = true;
            return;
        }

        SDL_Rect t;
        engine_slider_rect(g, &t);
        SDL_Rect hit = { t.x - 6, t.y - 8, t.w + 12, t.h + 16 };
        if (pt_in(&hit, p.x, p.y)) {
            g->eng_slider_drag = true;
            int v = eng_slider_from_x(g, p.x);
            if (v != g->eng_multipv) {
                g->eng_multipv = v;
                g->config_dirty = true;
                eval_restart(g);
            }
            return;
        }
    }

    SDL_Rect undo, restart, styles, pgn, menu;
    btn_rects(g, &undo, &restart);
    styles_btn_rect(g, &styles);
    pgn_btn_rect(g, &pgn);
    menu_btn_rect(g, &menu);
    if (pt_in(&undo, p.x, p.y)) {
        if (g->mode == MODE_LOCAL) set_msg(g, "Undo disabled in multiplayer", NULL);
        else do_undo(g);
        return;
    }
    if (pt_in(&restart, p.x, p.y)) {
        if (g->mode == MODE_LOCAL) set_msg(g, "Restart disabled in multiplayer", NULL);
        else do_restart(g);
        return;
    }
    if (pt_in(&styles, p.x, p.y)) { open_appearance(g, SCENE_GAME); return; }
    if (pt_in(&pgn, p.x, p.y)) { pgn_open_prompt(g); return; }
    if (pt_in(&menu, p.x, p.y)) { go_to_menu(g); return; }

    /* Dragging the board's corner grip resizes the board (and window). */
    SDL_Rect grip;
    board_grip_rect(g, &grip);
    if (pt_in(&grip, p.x, p.y)) {
        g->resizing_board = true;
        g->pan_x = g->pan_y = 0.0f;
        g->resize_start_zoom = g->zoom;
        g->resize_start_mx = g->mouse_win.x;
        g->resize_start_my = g->mouse_win.y;
        g->resize_start_bx = g->mouse.x;
        g->resize_start_by = g->mouse.y;
        return;
    }

    /* FEN controls are analysis-only. */
    if (g->mode == MODE_ANALYSIS) {
        SDL_Rect field, load, copy;
        fen_rects(g, &field, &load, &copy);
        if (pt_in(&load, p.x, p.y)) {
            g->fen_active = false;
            san_close_box(g);
            if (load_fen(g, g->fen_text)) set_msg(g, "Position loaded", NULL);
            else set_msg(g, "Invalid FEN", NULL);
            return;
        }
        if (pt_in(&copy, p.x, p.y)) { copy_fen_to_clipboard(g); return; }

        if (pt_in(&field, p.x, p.y)) {
            g->fen_active = true;
            san_close_box(g);
            return;
        }
        if (g->fen_active) { g->fen_active = false; fen_refresh(g); }
    } else {
        g->fen_active = false;
    }

    /* Clicking the SAN box reveals it; clicking elsewhere dismisses it. */
    SDL_Rect ib;
    input_rect(g, &ib);
    if (pt_in(&ib, p.x, p.y)) {
        if (!g->san_open) san_open_box(g);
        g->input.active = g->san_open;
        return;
    }
    if (g->san_open) san_close_box(g);

    if (g->promo_from >= 0) {
        SDL_Rect pr[4];
        promo_rects(g, pr);
        Piece choices[4] = { WQ, WR, WB, WN };
        for (int i = 0; i < 4; i++)
            if (pt_in(&pr[i], p.x, p.y)) { resolve_promotion(g, choices[i]); return; }
        g->promo_from = g->promo_to = -1;
        return;
    }

    int sq = sq_from_pos(g, p.x, p.y);
    if (sq < 0) return;
    ann_clear(g);   /* a left click on the board clears annotations */

    if (g->selected >= 0) {
        Move *m = target_move(g, sq);
        if (m) {
            try_move_to(g, sq);
            return;
        }
    }

    /* In singleplayer/local the player may only move their own side. */
    bool may_move = true;
    if (g->mode == MODE_SINGLE)     may_move = (g->board.side == g->human_color);
    else if (g->mode == MODE_LOCAL) may_move = (g->board.side == g->local_color);

    Piece pc = g->board.board[sq];
    bool my = (g->board.side == WHITE) ? WHITE_PIECE(pc) : BLACK_PIECE(pc);
    if (pc != EMPTY && my && may_move && g->state == NO_GAME_OVER) {
        if (sq == g->selected)
            clear_selection(g);
        else
            select_square(g, sq);
    } else {
        clear_selection(g);
    }
}

static void handle_game_mouseup(Gui *g)
{
    SDL_Point p = g->mouse;
    g->dragging = false;

    if (g->resizing_board) {
        g->resizing_board = false;
        g->pan_x = g->pan_y = 0.0f;      /* drop the temporary pan */
        apply_window_size(g);            /* refit the window to the new zoom */
        apply_render_scale(g);
        g->board_driven_resize = false;  /* don't swallow the next OS resize */
        g->pref_zoom = g->zoom;          /* remember the user's chosen size */
        g->config_dirty = true;          /* board_size persisted on exit */
        return;
    }

    if (g->promo_from >= 0) return; /* handled on down */

    int sq = sq_from_pos(g, p.x, p.y);
    if (sq < 0) return;
    if (g->selected >= 0) try_move_to(g, sq);
}

/*
 * Board-corner drag magnifies the whole UI. The window stays put during the
 * drag: the grabbed grip point is scaled about and panned so it stays exactly
 * under the cursor (window is refit on mouse-up). This removes the drift that a
 * single-axis zoom caused on the other axis.
 */
static void handle_board_resize(Gui *g)
{
    int dx = g->mouse_win.x - g->resize_start_mx;
    int dy = g->mouse_win.y - g->resize_start_my;

    int ax = g->resize_start_bx > 0 ? g->resize_start_bx : 8 * SQ_SIZE;
    int ay = g->resize_start_by > 0 ? g->resize_start_by : 8 * SQ_SIZE;
    float dz = (abs(dx) >= abs(dy)) ? (float)dx / (float)ax
                                    : (float)dy / (float)ay;

    float z = clamp_zoom(g->resize_start_zoom + dz);
    if (z == g->zoom) return;

    g->zoom = z;

    /* Recompute the transform at the new zoom with no pan, then offset it so
     * the grabbed base point maps exactly to the cursor (using the stored
     * transform values, so input and drawing stay in lock-step). */
    g->pan_x = g->pan_y = 0.0f;
    apply_render_scale(g);

    float s  = g->tf_scale > 0.0f ? g->tf_scale : 1.0f;
    float ux = g->tf_ux > 0.0f ? g->tf_ux : 1.0f;
    float uy = g->tf_uy > 0.0f ? g->tf_uy : 1.0f;

    g->pan_x = (float)g->mouse_win.x * ux / s
               - (float)g->resize_start_bx - g->tf_vpx;
    g->pan_y = (float)g->mouse_win.y * uy / s
               - (float)g->resize_start_by - g->tf_vpy;

    apply_render_scale(g);
    rebuild_fonts(g);
}

/* OS window resize: refit the base canvas (uniform magnification). */
static void handle_window_resize(Gui *g, int w, int h)
{
    if (g->board_driven_resize) {
        g->board_driven_resize = false;
        /* SDL resets the viewport on resize; re-install ours immediately so a
         * click arriving before the next frame still maps correctly. */
        apply_render_scale(g);
        return;
    }
    if (w <= 0 || h <= 0) return;

    g->pan_x = g->pan_y = 0.0f;   /* an OS resize cancels any drag pan */

    /* Display density may differ from gui_init_assets if the window moved. */
    int ow = w, oh = h;
    SDL_GetRendererOutputSize(g->ren, &ow, &oh);
    float density = (float)ow / (float)w;
    if (density < 1.0f) density = 1.0f;
    bool density_changed = (density != g->ui_scale);
    g->ui_scale = density;

    float z = fit_zoom((float)w, (float)h);
    bool zoom_changed = (z != g->zoom);
    g->zoom = z;

    apply_render_scale(g);
    if (zoom_changed || density_changed) rebuild_fonts(g);
}

static void debug_ui_log(Gui *g, int ex, int ey)
{
    if (!getenv("OPENCHESS_DEBUG_UI") || !g->ren || !g->win) return;
    float sx = 1.0f, sy = 1.0f;
    SDL_RenderGetScale(g->ren, &sx, &sy);
    SDL_Rect vp;
    SDL_RenderGetViewport(g->ren, &vp);
    int ow = 0, oh = 0, ww = 0, wh = 0;
    SDL_GetRendererOutputSize(g->ren, &ow, &oh);
    SDL_GetWindowSize(g->win, &ww, &wh);
    fprintf(stderr,
            "[ui] evt=(%d,%d) win=(%d,%d) base=(%d,%d) global=(%d,%d) winpos=(%d,%d) "
            "sdl_scale=(%.3f,%.3f) sdl_vp=(%d,%d,%d,%d) output=(%d,%d) window=(%d,%d) "
            "tf=(%.3f vp %.3f,%.3f u %.3f,%.3f) zoom=%.3f ui=%.3f\n",
            ex, ey, g->mouse_win.x, g->mouse_win.y, g->mouse.x, g->mouse.y,
            g->mouse_global.x, g->mouse_global.y, g->win_pos.x, g->win_pos.y,
            sx, sy, vp.x, vp.y, vp.w, vp.h, ow, oh, ww, wh,
            g->tf_scale, g->tf_vpx, g->tf_vpy, g->tf_ux, g->tf_uy,
            g->zoom, g->ui_scale);
}

void gui_handle_event(Gui *g, const SDL_Event *e)
{
    switch (e->type) {
        case SDL_QUIT:
            g->quit = true;
            return;
        case SDL_WINDOWEVENT:
            if (e->window.event == SDL_WINDOWEVENT_RESIZED ||
                e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                e->window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED) {
                int w = e->window.data1, h = e->window.data2;
                if (g->win) SDL_GetWindowSize(g->win, &w, &h);
                handle_window_resize(g, w, h);
            }
            return;
        case SDL_MOUSEMOTION:
            set_mouse(g, e->motion.x, e->motion.y);
            if (g->scene == SCENE_MENU) { handle_menu_mousemotion(g); return; }
            if (g->eng_slider_drag && (e->motion.state & SDL_BUTTON_LMASK)) {
                int v = eng_slider_from_x(g, g->mouse.x);
                if (v != g->eng_multipv) {
                    g->eng_multipv = v;
                    g->config_dirty = true;
                    eval_restart(g);
                }
                return;
            }
            if (g->resizing_board && (e->motion.state & SDL_BUTTON_LMASK)) {
                handle_board_resize(g);
                return;
            }
            if (g->scene == SCENE_GAME && g->ann_dragging &&
                (e->motion.state & SDL_BUTTON_RMASK)) {
                g->ann_to = sq_from_pos(g, g->mouse.x, g->mouse.y);
                return;
            }
            if (e->motion.state & SDL_BUTTON_LMASK) {
                if (g->selected >= 0 || g->promo_from >= 0) g->dragging = true;
            }
            return;
        case SDL_MOUSEBUTTONDOWN:
            if (e->button.button == SDL_BUTTON_RIGHT) {
                set_mouse(g, e->button.x, e->button.y);
                if (g->scene == SCENE_GAME) {
                    int sq = sq_from_pos(g, g->mouse.x, g->mouse.y);
                    if (sq >= 0) {
                        g->ann_dragging = true;
                        g->ann_from = sq;
                        g->ann_to = sq;
                    }
                }
                return;
            }
            if (e->button.button != SDL_BUTTON_LEFT) return;
            set_mouse(g, e->button.x, e->button.y);
            debug_ui_log(g, e->button.x, e->button.y);
            if (g->scene == SCENE_MENU) handle_menu_mousedown(g);
            else if (g->scene == SCENE_SINGLE_SETUP) handle_setup_mousedown(g);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_mousedown(g);
            else if (g->scene == SCENE_APPEARANCE) handle_appearance_mousedown(g);
            else if (g->scene == SCENE_SETTINGS) handle_settings_mousedown(g);
            else handle_game_mousedown(g);
            return;
        case SDL_MOUSEBUTTONUP:
            if (e->button.button == SDL_BUTTON_RIGHT) {
                set_mouse(g, e->button.x, e->button.y);
                if (g->scene == SCENE_GAME && g->ann_dragging) {
                    int sq = sq_from_pos(g, g->mouse.x, g->mouse.y);
                    if (sq < 0) sq = g->ann_from;
                    Uint8 r, gg, b;
                    ann_color_for(&r, &gg, &b);
                    if (sq == g->ann_from) {
                        int found = -1;
                        for (int i = 0; i < g->ann_square_count; i++)
                            if (g->ann_squares[i].sq == sq) { found = i; break; }
                        if (found >= 0) {
                            g->ann_squares[found] =
                                g->ann_squares[--g->ann_square_count];
                        } else if (g->ann_square_count < MAX_ANN) {
                            AnnSquare *c = &g->ann_squares[g->ann_square_count++];
                            c->sq = sq; c->r = r; c->g = gg; c->b = b;
                        }
                    } else {
                        int found = -1;
                        for (int i = 0; i < g->ann_arrow_count; i++)
                            if (g->ann_arrows[i].from == g->ann_from &&
                                g->ann_arrows[i].to == sq) { found = i; break; }
                        if (found >= 0) {
                            g->ann_arrows[found] =
                                g->ann_arrows[--g->ann_arrow_count];
                        } else if (g->ann_arrow_count < MAX_ANN) {
                            AnnArrow *a = &g->ann_arrows[g->ann_arrow_count++];
                            a->from = g->ann_from; a->to = sq;
                            a->r = r; a->g = gg; a->b = b;
                        }
                    }
                    g->ann_dragging = false;
                    g->ann_from = g->ann_to = -1;
                }
                return;
            }
            if (e->button.button != SDL_BUTTON_LEFT) return;
            set_mouse(g, e->button.x, e->button.y);
            if (g->scene == SCENE_GAME && g->eng_slider_drag) {
                g->eng_slider_drag = false;
                return;
            }
            if (g->scene == SCENE_GAME) handle_game_mouseup(g);
            return;
        case SDL_KEYDOWN:
            if (g->scene == SCENE_MENU) handle_menu_keydown(g, &e->key);
            else if (g->scene == SCENE_SINGLE_SETUP) handle_setup_keydown(g, &e->key);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_keydown(g, &e->key);
            else if (g->scene == SCENE_APPEARANCE) handle_appearance_keydown(g, &e->key);
            else if (g->scene == SCENE_SETTINGS) handle_settings_keydown(g, &e->key);
            else handle_game_keydown(g, &e->key);
            return;
        case SDL_TEXTINPUT:
            if (g->scene == SCENE_GAME) handle_game_textinput(g, &e->text);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_textinput(g, &e->text);
            else if (g->scene == SCENE_SETTINGS) handle_engine_textinput(g, &e->text);
            return;
        default:
            return;
    }
}

static void single_tick(Gui *g, Uint32 now)
{
    if (!g->ai || !ai_alive(g->ai)) {
        g->ai_thinking = false;
        return;
    }

    if (g->ai_thinking) {
        char uci[8];
        if (ai_poll_bestmove(g->ai, uci)) {
            g->ai_thinking = false;
            Move m;
            if (ai_uci_to_move(&g->board, uci, &m))
                push_move(g, m);
        }
        return;
    }

    /* let the engine move whenever it is its turn */
    if (g->state == NO_GAME_OVER && g->board.side != g->human_color) {
        ai_go(g->ai, &g->board);
        g->ai_thinking = true;
        g->ai_think_start = now;
    }
}

static void local_tick(Gui *g)
{
    if (!g->net) return;

    char line[256];
    int r;
    while ((r = net_poll(g->net, line, sizeof line)) == 1) {
        if (strncmp(line, "MOVE ", 5) == 0) {
            Move m;
            if (g->board.side != g->local_color &&
                ai_uci_to_move(&g->board, line + 5, &m)) {
                push_move(g, m);
                g->net_sent_ply = g->ply;   /* remote move: never echo */
            }
        } else if (strncmp(line, "BYE", 3) == 0) {
            r = -1;
            break;
        }
        /* HELLO / FEN are informational in this phase */
    }

    if (r < 0) {
        set_msg(g, "Opponent disconnected", NULL);
        net_close(g->net);
        g->net = NULL;
        go_to_menu(g);
        return;
    }

    /* forward any moves made locally */
    while (g->net_sent_ply < g->ply) {
        char uci[8];
        move_to_uci(g->history[g->net_sent_ply], uci);
        char msg[32];
        snprintf(msg, sizeof msg, "MOVE %s", uci);
        net_send(g->net, msg);
        g->net_sent_ply++;
    }
}

static void hostjoin_tick(Gui *g)
{
    if (!g->net) return;

    char line[256];
    int r;
    while ((r = net_poll(g->net, line, sizeof line)) == 1) {
        /* ignore the peer's HELLO/FEN; sides are derived from host/join role */
    }
    if (r < 0) {
        net_close(g->net);
        g->net = NULL;
        g->net_waiting = false;
        snprintf(g->menu_msg, sizeof g->menu_msg, "Connection closed");
        return;
    }
    if (net_state(g->net) == NET_STATE_CONNECTED) {
        Color side = (net_role(g->net) == NET_ROLE_HOST) ? WHITE : BLACK;
        enter_local_game(g, side);
    }
}

void gui_tick(Gui *g, Uint32 now)
{
    if (g->scene == SCENE_HOSTJOIN) {
        hostjoin_tick(g);
        return;
    }
    if (g->scene != SCENE_GAME) return;

    if (g->mode == MODE_SINGLE) {
        single_tick(g, now);
    } else if (g->mode == MODE_LOCAL) {
        local_tick(g);
    } else if (g->eval_ai) {
        /* live analysis: drain info lines and refresh the evaluation */
        char uci[8];
        bool done = ai_poll_bestmove(g->eval_ai, uci);
        int cp, mate, depth;
        if (ai_get_eval(g->eval_ai, &cp, &mate, &depth)) {
            if (g->eval_side == BLACK) { cp = -cp; mate = -mate; }
            g->eval_cp = cp;
            g->eval_mate = mate;
            g->eval_depth = depth;
            g->eval_has_mate = ai_eval_has_mate(g->eval_ai);
            g->eval_valid = true;
        }
        g->eng_line_count = ai_get_lines(g->eval_ai, g->eng_lines, AI_MAX_LINES);
        /* A capped search ends on bestmove; keep the analysis cycling. */
        if (done && (g->eng_depth > 0 || g->eng_time_ms > 0))
            eval_restart(g);
    }
}

/* ------------------------------------------------------------------ */
/* rendering                                                           */
/* ------------------------------------------------------------------ */

static float ease_out_back(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}

/* Compute the flying piece transform for a given animation style. */
static void anim_piece_transform(const Gui *g, AnimStyle style, float t,
                                 float fx, float fy, float tx, float ty,
                                 float *x, float *y, float *scale, float *alpha)
{
    switch (style) {
        case ANIM_ARCADE: {
            float e = ease_out_back(t);
            *x = fx + (tx - fx) * e;
            *y = fy + (ty - fy) * e - 0.16f * g->sq * sinf(GUI_PI * t);
            *scale = 1.0f + 0.12f * sinf(GUI_PI * t);
            *alpha = 1.0f;
            break;
        }
        case ANIM_SLIDE: {
            float e = t * t * (3.0f - 2.0f * t);
            *x = fx + (tx - fx) * e;
            *y = fy + (ty - fy) * e;
            *scale = 1.0f;
            *alpha = 1.0f;
            break;
        }
        case ANIM_FADE:
        default:
            *x = tx; *y = ty; *scale = 1.0f; *alpha = t;
            break;
    }
}

static void captured_transform(AnimStyle style, float t, float *scale, float *alpha)
{
    if (style == ANIM_ARCADE) {
        float ct = t < 0.5f ? t / 0.5f : 1.0f;
        *scale = 1.0f - 0.35f * ct;
        *alpha = 1.0f - ct;
    } else {
        *scale = 1.0f;
        *alpha = 1.0f - t;
    }
}

static void render_piece_box(Gui *g, SDL_Renderer *ren, Piece p,
                             float cx, float cy, float size, float alpha)
{
    if (p <= EMPTY || p > BK) return;
    SDL_Texture *t = g->tex_piece[p];
    if (!t) return;
    int w, h;
    SDL_QueryTexture(t, NULL, NULL, &w, &h);
    if (w <= 0 || h <= 0) return;
    int m = w > h ? w : h;
    float s = size / (float)m;
    SDL_Rect rc = { (int)(cx - w * 0.5f * s), (int)(cy - h * 0.5f * s),
                    (int)(w * s), (int)(h * s) };
    SDL_SetTextureAlphaMod(t, (Uint8)(alpha * 255.0f));
    SDL_RenderCopy(ren, t, NULL, &rc);
    SDL_SetTextureAlphaMod(t, 255);
}

static void draw_board_squares(Gui *g, SDL_Renderer *ren)
{
    ThemeEntry *bt = cur_board(g);
    if (g->tex_board) {
        SDL_Rect dst = { g->board_x, g->board_y, 8 * g->sq, 8 * g->sq };
        SDL_RenderCopy(ren, g->tex_board, NULL, &dst);
        return;
    }
    unsigned char *light = bt ? bt->light : (unsigned char[]){ 240, 217, 181 };
    unsigned char *dark  = bt ? bt->dark  : (unsigned char[]){ 181, 136, 99 };
    for (int rank = 0; rank < 8; rank++)
        for (int file = 0; file < 8; file++) {
            SDL_Rect rc = { g->board_x + file * g->sq, g->board_y + (7 - rank) * g->sq,
                            g->sq, g->sq };
            if ((rank + file) % 2 == 0)
                set_render_color(ren, light[0], light[1], light[2]);
            else
                set_render_color(ren, dark[0], dark[1], dark[2]);
            SDL_RenderFillRect(ren, &rc);
        }
}

static void draw_coordinates(Gui *g)
{
    ThemeEntry *bt = cur_board(g);
    unsigned char *cc = bt ? bt->coord : (unsigned char[]){ 40, 30, 20 };
    SDL_Color cdark = { cc[0], cc[1], cc[2], 255 };
    SDL_Color clite = { 245, 250, 252, 255 };

    /* Labels follow the current orientation: derive them from the screen
     * cell's underlying square and contrast against that square's color. */
    for (int col = 0; col < 8; col++) {
        int sq = screen_sq(g, col, 7);
        int file = sq % 8, rank = sq / 8;
        char c[2] = { (char)('a' + file), 0 };
        SDL_Color color = ((file + rank) % 2 == 0) ? cdark : clite;
        render_text(g, g->font_small, c,
                    g->board_x + col * g->sq + g->sq - 14,
                    g->board_y + 7 * g->sq + g->sq - 20, color);
    }
    for (int row = 0; row < 8; row++) {
        int sq = screen_sq(g, 0, row);
        int file = sq % 8, rank = sq / 8;
        char c[2] = { (char)('1' + rank), 0 };
        SDL_Color color = ((file + rank) % 2 == 0) ? cdark : clite;
        render_text(g, g->font_small, c, g->board_x + 5,
                    g->board_y + row * g->sq + 4, color);
    }
}

/* True if the static board piece on `sq` must be suppressed because an
 * animation owns that square. */
static bool sq_hidden_by_anim(const Gui *g, int sq)
{
    for (int i = 0; i < g->anim_count; i++) {
        const AnimStep *s = &g->anim_queue[i];
        if (s->to == sq) return true;
        if (s->has_rook && s->rto == sq) return true;
        if (i > 0) {  /* pending steps still show their pre-move picture */
            if (s->from == sq) return true;
            if (s->cap_sq >= 0 && s->cap_sq == sq) return true;
        }
    }
    return false;
}

static void draw_piece_layer(Gui *g, SDL_Renderer *ren, Uint32 now)
{
    /* static pieces (minus animated squares) */
    for (int r = 0; r < 8; r++)
        for (int f = 0; f < 8; f++) {
            int sq = r * 8 + f;
            if (sq_hidden_by_anim(g, sq)) continue;
            /* While dragging, the piece follows the cursor; leave the board
             * texture visible on its origin square. */
            if (g->dragging && sq == g->selected) continue;
            Piece p = g->board.board[sq];
            if (p == EMPTY) continue;
            float cx, cy;
            piece_center(g, sq, &cx, &cy);
            render_piece_box(g, ren, p, cx, cy, g->sq, 1.0f);
        }

    /* queued-but-pending moves: draw the position before those moves */
    for (int i = g->anim_count - 1; i >= 1; i--) {
        AnimStep *s = &g->anim_queue[i];
        float cx, cy;
        piece_center(g, s->from, &cx, &cy);
        render_piece_box(g, ren, s->piece, cx, cy, g->sq, 1.0f);
        if (s->cap_sq >= 0 && s->captured != EMPTY) {
            piece_center(g, s->cap_sq, &cx, &cy);
            render_piece_box(g, ren, s->captured, cx, cy, g->sq, 1.0f);
        }
    }

    /* currently animating move */
    if (g->anim_count > 0) {
        AnimStep *s = &g->anim_queue[0];
        float t = (s->dur > 0) ? (float)(now - s->start) / (float)s->dur : 1.0f;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        if (s->cap_sq >= 0 && s->captured != EMPTY) {
            float cx, cy, sc, al;
            piece_center(g, s->cap_sq, &cx, &cy);
            captured_transform(g->anim_style, t, &sc, &al);
            render_piece_box(g, ren, s->captured, cx, cy, g->sq * sc, al);
        }

        float fx, fy, tx, ty, x, y, sc, al;
        piece_center(g, s->from, &fx, &fy);
        piece_center(g, s->to, &tx, &ty);
        anim_piece_transform(g, g->anim_style, t, fx, fy, tx, ty, &x, &y, &sc, &al);
        render_piece_box(g, ren, s->piece, x, y, g->sq * sc, al);

        if (s->has_rook) {
            piece_center(g, s->rfrom, &fx, &fy);
            piece_center(g, s->rto, &tx, &ty);
            anim_piece_transform(g, g->anim_style, t, fx, fy, tx, ty, &x, &y, &sc, &al);
            render_piece_box(g, ren, s->rook, x, y, g->sq * sc, al);
        }
    }
}

static void draw_square_highlight(Gui *g, SDL_Renderer *ren, int sq,
                                  Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    set_render_color(ren, r, gg, b);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void draw_target_dot(Gui *g, SDL_Renderer *ren, int sq, bool capture)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    set_render_color(ren, 40, 90, 40);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (capture) {
        SDL_Rect ring = { rc.x + 4, rc.y + 4, g->sq - 8, g->sq - 8 };
        SDL_RenderDrawRect(ren, &ring);
    } else {
        int cr = 12;
        SDL_Rect dot = { rc.x + g->sq / 2 - cr, rc.y + g->sq / 2 - cr, cr * 2, cr * 2 };
        SDL_RenderFillRect(ren, &dot);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* ---- right-mouse annotations ---- */

/* Default amber; Ctrl = green, Alt = blue. */
static void ann_color_for(Uint8 *r, Uint8 *g, Uint8 *b)
{
    SDL_Keymod m = SDL_GetModState();
    if (m & KMOD_CTRL)     { *r = 70;  *g = 200; *b = 90;  }
    else if (m & KMOD_ALT) { *r = 70;  *g = 150; *b = 245; }
    else                   { *r = 235; *g = 150; *b = 40;  }
}

#if !SDL_VERSION_ATLEAST(2, 0, 18)
/* Fallback for SDL < 2.0.18: parallel 1px lines. */
static void draw_thick_line(SDL_Renderer *ren, float x0, float y0,
                            float x1, float y1, float w)
{
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;
    float px = -dy / len * w, py = dx / len * w;
    int n = (int)w + 1;
    for (int i = -n; i <= n; i++) {
        float t = (float)i / (float)n;
        SDL_RenderDrawLine(ren,
                           (int)(x0 + px * t), (int)(y0 + py * t),
                           (int)(x1 + px * t), (int)(y1 + py * t));
    }
}
#endif

/* A solid arrow drawn as one filled polygon (shaft quad + head triangle) so it
 * scales with the UI without the gaps that unstyled thin lines produce. */
static void draw_arrow_solid(SDL_Renderer *ren, float fx, float fy,
                             float tx, float ty, float sq)
{
    if (sq <= 1.0f) return;
    float dx = tx - fx, dy = ty - fy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;
    float ux = dx / len, uy = dy / len;
    float ax = fx + ux * sq * 0.16f, ay = fy + uy * sq * 0.16f;  /* shaft start */
    float bx = tx - ux * sq * 0.34f, by = ty - uy * sq * 0.34f;  /* head base */
    float px = -uy, py = ux;                                     /* perpendicular */
    float sw = sq * 0.085f;   /* shaft half-width */
    float hw = sq * 0.30f;    /* head half-width */
    float ov = sq * 0.05f;    /* overlap into the head to avoid a seam */

#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_Vertex v[7];
    v[0].position = (SDL_FPoint){ ax + px * sw, ay + py * sw };
    v[1].position = (SDL_FPoint){ bx + ux * ov + px * sw, by + uy * ov + py * sw };
    v[2].position = (SDL_FPoint){ bx + ux * ov - px * sw, by + uy * ov - py * sw };
    v[3].position = (SDL_FPoint){ ax - px * sw, ay - py * sw };
    v[4].position = (SDL_FPoint){ tx, ty };
    v[5].position = (SDL_FPoint){ bx + px * hw, by + py * hw };
    v[6].position = (SDL_FPoint){ bx - px * hw, by - py * hw };

    SDL_Color c;
    SDL_GetRenderDrawColor(ren, &c.r, &c.g, &c.b, &c.a);
    for (int i = 0; i < 7; i++) {
        v[i].color = c;
        v[i].tex_coord = (SDL_FPoint){ 0, 0 };
    }
    const int idx[9] = { 0, 1, 2, 0, 2, 3, 4, 5, 6 };
    SDL_RenderGeometry(ren, NULL, v, 7, idx, 9);
#else
    draw_thick_line(ren, ax, ay, bx, by, sw);
    draw_thick_line(ren, tx, ty, bx + px * hw, by + py * hw, sw);
    draw_thick_line(ren, tx, ty, bx - px * hw, by - py * hw, sw);
    draw_thick_line(ren, bx + px * hw, by + py * hw, bx - px * hw, by - py * hw, sw);
#endif
}

static void draw_one_arrow(Gui *g, SDL_Renderer *ren, int from, int to,
                           Uint8 r, Uint8 gg, Uint8 b)
{
    if (from < 0 || to < 0 || from == to) return;
    float fx, fy, tx, ty;
    piece_center(g, from, &fx, &fy);
    piece_center(g, to, &tx, &ty);
    set_render_color(ren, r, gg, b);
    draw_arrow_solid(ren, fx, fy, tx, ty, (float)g->sq);
}

/* Translucent square with a stronger border, used for right-click marks. */
static void draw_annotation_square(Gui *g, SDL_Renderer *ren,
                                   int sq, Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, r, gg, b, 70);
    SDL_RenderFillRect(ren, &rc);

    int t = (int)(g->sq * 0.10f);
    if (t < 2) t = 2;
    SDL_SetRenderDrawColor(ren, r, gg, b, 205);
    SDL_Rect top   = { rc.x, rc.y, rc.w, t };
    SDL_Rect bot   = { rc.x, rc.y + rc.h - t, rc.w, t };
    SDL_Rect left  = { rc.x, rc.y, t, rc.h };
    SDL_Rect right = { rc.x + rc.w - t, rc.y, t, rc.h };
    SDL_RenderFillRect(ren, &top);
    SDL_RenderFillRect(ren, &bot);
    SDL_RenderFillRect(ren, &left);
    SDL_RenderFillRect(ren, &right);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* Green arrow for the first move of each engine line (analysis). */
static void draw_engine_arrows(Gui *g, SDL_Renderer *ren)
{
    if (g->mode != MODE_ANALYSIS || !g->engine_arrows) return;
    if (g->eng_multipv <= 0 || g->eng_line_count <= 0) return;

    for (int i = 0; i < g->eng_line_count && i < AI_MAX_LINES; i++) {
        const char *pv = g->eng_lines[i].pv;
        while (*pv == ' ') pv++;
        if (!*pv) continue;
        char tok[8];
        int n = 0;
        while (pv[n] && pv[n] != ' ' && n < 7) { tok[n] = pv[n]; n++; }
        tok[n] = '\0';

        Move m;
        if (ai_uci_to_move(&g->board, tok, &m))
            draw_one_arrow(g, ren, MOVE_FROM(m), MOVE_TO(m), 70, 200, 90);
    }
}

static void draw_annotations(Gui *g, SDL_Renderer *ren)
{
    if (g->ann_square_count == 0 && g->ann_arrow_count == 0 && !g->ann_dragging)
        return;

    for (int i = 0; i < g->ann_square_count; i++) {
        AnnSquare *c = &g->ann_squares[i];
        draw_annotation_square(g, ren, c->sq, c->r, c->g, c->b);
    }

    for (int i = 0; i < g->ann_arrow_count; i++) {
        AnnArrow *a = &g->ann_arrows[i];
        draw_one_arrow(g, ren, a->from, a->to, a->r, a->g, a->b);
    }

    if (g->ann_dragging && g->ann_from >= 0 && g->ann_to >= 0 &&
        g->ann_to != g->ann_from) {
        Uint8 r, gg, b;
        ann_color_for(&r, &gg, &b);
        draw_one_arrow(g, ren, g->ann_from, g->ann_to, r, gg, b);
    }
}

static void draw_promo_chooser(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect pr[4];
    promo_rects(g, pr);
    for (int i = 0; i < 4; i++) {
        draw_rect(ren, &pr[i], 60, 40, 40, true);
        draw_rect(ren, &pr[i], 230, 210, 160, false);
    }
    Piece choices[4] = { WQ, WR, WB, WN };
    for (int i = 0; i < 4; i++) {
        Piece p = color_of_piece(g->board.board[g->promo_from]) ? choices[i]
                 : (Piece)(choices[i] + (BP - WN));
        float cx = pr[i].x + pr[i].w / 2.0f;
        float cy = pr[i].y + pr[i].h / 2.0f;
        render_piece_box(g, ren, p, cx, cy, (float)(pr[i].h - 8), 1.0f);
    }
}

static void render_status(Gui *g)
{
    char line[64];
    if (g->state == CHECKMATE)
        snprintf(line, sizeof line, "Checkmate - %s wins",
                 g->board.side == WHITE ? "Black" : "White");
    else if (g->state == STALEMATE)
        snprintf(line, sizeof line, "Stalemate");
    else if (g->state == INSUFFICIENT_MATERIAL)
        snprintf(line, sizeof line, "Draw (insufficient material)");
    else {
        snprintf(line, sizeof line, "%s to move%s",
                 g->board.side == WHITE ? "White" : "Black",
                 in_check(&g->board, g->board.side) ? " - CHECK" : "");
    }
    render_text(g, g->font_ui, line, g->panel_x, g->board_y, (SDL_Color){ 240, 220, 130, 255 });

    if (g->mode == MODE_SINGLE) {
        char info[96];
        snprintf(info, sizeof info, "You: %s    AI: %s%s",
                 g->human_color == WHITE ? "White" : "Black",
                 AI_LEVELS[g->setup_level].label,
                 g->ai_thinking ? "   (thinking...)" : "");
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 150, 210, 230, 255 });
    } else if (g->mode == MODE_LOCAL) {
        bool ok = g->net && net_state(g->net) == NET_STATE_CONNECTED;
        char info[96];
        snprintf(info, sizeof info, "You: %s    %s",
                 g->local_color == WHITE ? "White" : "Black",
                 ok ? "opponent connected" : "opponent disconnected");
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 150, 210, 230, 255 });
    } else if (g->mode == MODE_ANALYSIS) {
        char info[96];
        if (!g->eval_ai)
            snprintf(info, sizeof info, "Analysis: no engine");
        else if (!g->eval_valid)
            snprintf(info, sizeof info, "Analysis: thinking...");
        else if (g->eval_has_mate && g->eval_mate == 0)
            snprintf(info, sizeof info, "Eval: checkmate (%s wins)",
                     g->eval_side == WHITE ? "Black" : "White");
        else if (g->eval_has_mate)
            snprintf(info, sizeof info, "Eval: %sM%d   (depth %d)",
                     g->eval_mate > 0 ? "+" : "-",
                     g->eval_mate > 0 ? g->eval_mate : -g->eval_mate,
                     g->eval_depth);
        else
            snprintf(info, sizeof info, "Eval: %+.2f   (depth %d)",
                     g->eval_cp / 100.0, g->eval_depth);
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 150, 210, 230, 255 });
    }
}

/* Vertical evaluation bar to the left of the board (analysis mode). */
static void draw_eval_bar(Gui *g, SDL_Renderer *ren)
{
    if (g->mode != MODE_ANALYSIS || !g->eval_valid) return;

    int x = 2, w = 10;
    int y = g->board_y, h = 8 * g->sq;

    set_render_color(ren, 25, 25, 30);
    SDL_Rect bg = { x, y, w, h };
    SDL_RenderFillRect(ren, &bg);

    float f;
    if (g->eval_has_mate) {
        if (g->eval_mate > 0)      f = 1.0f;   /* White mates */
        else if (g->eval_mate < 0) f = 0.0f;   /* Black mates */
        else f = (g->eval_side == WHITE) ? 0.0f : 1.0f;  /* side to move is mated */
    } else {
        f = 1.0f / (1.0f + expf(-(float)g->eval_cp / 400.0f));
    }

    int wh = (int)(h * f);
    SDL_Rect white = { x, y + h - wh, w, wh };
    set_render_color(ren, 235, 235, 235);
    SDL_RenderFillRect(ren, &white);

    SDL_Rect border = { x, y, w, h };
    set_render_color(ren, 130, 130, 140);
    SDL_RenderDrawRect(ren, &border);
}

/* Convert a UCI principal variation into a short SAN sequence. */
static void pv_to_san(const Gui *g, const char *pv, char *out, size_t n)
{
    Board b = g->board;
    out[0] = '\0';
    size_t used = 0;
    int move_no = g->ply / 2 + 1;
    bool white = (g->board.side == WHITE);
    int count = 0;

    const char *p = pv;
    while (*p && count < 8) {
        while (*p == ' ') p++;
        char tok[8];
        int i = 0;
        while (*p && *p != ' ' && i < 7) tok[i++] = *p++;
        tok[i] = '\0';
        if (i < 4) break;

        Move m;
        if (!ai_uci_to_move(&b, tok, &m)) break;
        char san[16], piece[40];
        move_to_san(&b, m, san, sizeof san);
        if (white) snprintf(piece, sizeof piece, "%d.%s ", move_no, san);
        else       snprintf(piece, sizeof piece, "%s ", san);
        if (used + strlen(piece) + 1 >= n) break;
        strcat(out, piece);
        used += strlen(piece);

        make_move_plumb(&b, m);
        white = !white;
        if (white) move_no++;
        count++;
    }
}

static void render_engine_panel(Gui *g, SDL_Renderer *ren)
{
    if (g->mode != MODE_ANALYSIS) return;

    render_text(g, g->font_small, "Engine lines", g->panel_x, g->board_y + 52,
                (SDL_Color){ 200, 200, 200, 255 });

    SDL_Rect ab;
    engine_arrows_rect(g, &ab);
    draw_rect(ren, &ab, 40, 40, 46, true);
    draw_rect(ren, &ab, 120, 120, 130, false);
    if (g->engine_arrows) {
        set_render_color(ren, 80, 200, 100);
        SDL_Rect f = { ab.x + 3, ab.y + 3, ab.w - 6, ab.h - 6 };
        SDL_RenderFillRect(ren, &f);
    }
    render_text(g, g->font_small, "Arrows", ab.x + ab.w + 6, ab.y,
                (SDL_Color){ 200, 200, 200, 255 });

    SDL_Rect t;
    engine_slider_rect(g, &t);
    draw_rect(ren, &t, 45, 45, 50, true);
    int fillw = t.w * g->eng_multipv / AI_MAX_LINES;
    SDL_Rect fill = { t.x, t.y, fillw, t.h };
    set_render_color(ren, 90, 150, 200);
    SDL_RenderFillRect(ren, &fill);
    set_render_color(ren, 220, 220, 220);
    SDL_Rect knob = { t.x + fillw - 4, t.y - 3, 8, t.h + 6 };
    SDL_RenderFillRect(ren, &knob);
    draw_rect(ren, &t, 120, 120, 130, false);

    char val[16];
    if (g->eng_multipv == 0) snprintf(val, sizeof val, "off");
    else snprintf(val, sizeof val, "%d", g->eng_multipv);
    render_text(g, g->font_small, val, t.x + t.w + 10, t.y - 2,
                (SDL_Color){ 210, 210, 210, 255 });

    int y = t.y + 26;
    for (int i = 0; i < g->eng_line_count && i < AI_MAX_LINES; i++) {
        AiLine *L = &g->eng_lines[i];
        char sc[24], pvs[160], line[220];
        if (L->has_mate)
            snprintf(sc, sizeof sc, "%sM%d", L->mate >= 0 ? "+" : "-",
                     L->mate >= 0 ? L->mate : -L->mate);
        else
            snprintf(sc, sizeof sc, "%+.2f", L->cp / 100.0);
        pv_to_san(g, L->pv, pvs, sizeof pvs);
        snprintf(line, sizeof line, "%d. %s  %s", i + 1, sc, pvs);
        render_text(g, g->font_small, line, g->panel_x, y,
                    (SDL_Color){ 205, 215, 225, 255 });
        y += 18;
    }
}

static void render_move_list(Gui *g)
{
    SDL_Color c = { 230, 225, 215, 255 };
    bool analysis = (g->mode == MODE_ANALYSIS);
    int max_rows = analysis ? 9 : 16;
    int y = analysis ? g->board_y + 218 : g->board_y + 78;
    render_text(g, g->font_small, "Moves:", g->panel_x, y - 22, c);
    int start = g->ply > max_rows ? g->ply - max_rows : 0;
    for (int i = start; i < g->ply; i++) {
        int num = i / 2 + 1;
        if (i % 2 == 0) {
            char header[16];
            snprintf(header, sizeof header, "%d.", num);
            render_text(g, g->font_small, header, g->panel_x, y, (SDL_Color){ 150, 150, 150, 255 });
            render_text(g, g->font_small, g->move_san[i], g->panel_x + 42, y, c);
        } else {
            render_text(g, g->font_small, g->move_san[i], g->panel_x + 150, y, c);
        }
        y += 20;
    }
}

static void render_san_hint(Gui *g)
{
    SDL_Rect ib;
    input_rect(g, &ib);
    render_text(g, g->font_small, "Press Enter to type a move", g->panel_x, ib.y + 12,
                (SDL_Color){ 120, 130, 140, 255 });
}

static void render_input_box(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect ib;
    input_rect(g, &ib);
    render_text(g, g->font_small, "Type a move (SAN):", g->panel_x, ib.y - 22,
                (SDL_Color){ 200, 200, 200, 255 });
    draw_rect(ren, &ib, 30, 30, 32, true);
    draw_rect(ren, &ib, 60, 60, 70, false);
    if (g->input.len > 0) {
        render_text(g, g->font_ui, g->input.text, ib.x + 10, ib.y + 8,
                    (SDL_Color){ 240, 240, 240, 255 });
        int tw = 0;
        SDL_Surface *s = TTF_RenderUTF8_Blended(g->font_ui, g->input.text,
                                                (SDL_Color){ 255, 255, 255, 255 });
        if (s) {
            float sc = eff_scale(g);
            tw = (int)lroundf(s->w / sc);
            SDL_FreeSurface(s);
        }
        set_render_color(ren, 240, 240, 240);
        SDL_Rect caret = { ib.x + 12 + tw, ib.y + 8, 2, ib.h - 16 };
        SDL_RenderFillRect(ren, &caret);
    } else {
        render_text(g, g->font_small, "e.g. e4, Nf3, O-O-O", ib.x + 10, ib.y + 12,
                    (SDL_Color){ 120, 120, 120, 255 });
        set_render_color(ren, 120, 120, 120);
        SDL_Rect caret = { ib.x + 12, ib.y + 8, 2, ib.h - 16 };
        SDL_RenderFillRect(ren, &caret);
    }
}

static void render_fen_box(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect field, load, copy;
    fen_rects(g, &field, &load, &copy);

    render_text(g, g->font_small, "FEN:", g->panel_x, field.y - 20,
                (SDL_Color){ 200, 200, 200, 255 });

    draw_rect(ren, &field, 30, 30, 32, true);
    if (g->fen_active)
        draw_rect(ren, &field, 90, 140, 200, false);
    else
        draw_rect(ren, &field, 60, 60, 70, false);

    SDL_RenderSetClipRect(ren, &field);
    render_text(g, g->font_small, g->fen_text, field.x + 8, field.y + 10,
                (SDL_Color){ 220, 225, 230, 255 });
    if (g->fen_active) {
        int w = 0, h = 0;
        TTF_SizeUTF8(g->font_small, g->fen_text, &w, &h);
        float sc = eff_scale(g);
        int lw = (int)lroundf(w / sc);
        int cx = field.x + 8 + lw;
        if (cx > field.x + field.w - 3) cx = field.x + field.w - 3;
        set_render_color(ren, 240, 240, 240);
        SDL_Rect caret = { cx, field.y + 8, 2, field.h - 16 };
        SDL_RenderFillRect(ren, &caret);
    }
    SDL_RenderSetClipRect(ren, NULL);

    draw_rect(ren, &load, 55, 60, 70, true);
    draw_rect(ren, &load, 120, 120, 130, false);
    render_text(g, g->font_small, "Load", load.x + 11, load.y + 10,
                (SDL_Color){ 230, 230, 230, 255 });

    draw_rect(ren, &copy, 55, 60, 70, true);
    draw_rect(ren, &copy, 120, 120, 130, false);
    render_text(g, g->font_small, "Copy", copy.x + 9, copy.y + 10,
                (SDL_Color){ 230, 230, 230, 255 });
}

static void render_text_centered_rect_f(Gui *g, SDL_Renderer *ren, SDL_Rect *r,
                                        TTF_Font *font, const char *label)
{
    draw_rect(ren, r, 45, 45, 50, true);
    draw_rect(ren, r, 120, 120, 130, false);
    int ty = r->y + (r->h - text_height(g, font)) / 2;
    render_text_centered(g, font, label, r->x + r->w / 2, ty,
                         (SDL_Color){ 230, 230, 230, 255 });
}

static void render_text_centered_rect(Gui *g, SDL_Renderer *ren, SDL_Rect *r,
                                      const char *label)
{
    render_text_centered_rect_f(g, ren, r, g->font_ui, label);
}

static void render_buttons(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect undo, restart, styles, pgn, menu;
    btn_rects(g, &undo, &restart);
    styles_btn_rect(g, &styles);
    pgn_btn_rect(g, &pgn);
    menu_btn_rect(g, &menu);
    render_text_centered_rect(g, ren, &undo, "Undo");
    render_text_centered_rect(g, ren, &restart, "Restart");
    render_text_centered_rect(g, ren, &styles, "Styles");
    render_text_centered_rect_f(g, ren, &pgn, g->font_small, "Save PGN");
    render_text_centered_rect(g, ren, &menu, "Menu");
    render_text(g, g->font_tiny, "Ctrl+U undo  Ctrl+R restart  Ctrl+S save PGN  Ctrl+F flip",
                g->panel_x, undo.y + 48, (SDL_Color){ 130, 130, 130, 255 });
}

static void render_hud(Gui *g)
{
    char line[192];
    snprintf(line, sizeof line, "Board: %s   Pieces: %s   Anim: %s",
             theme_label(&g->boards, g->board_index),
             theme_label(&g->pieces, g->piece_index),
             ANIM_LABELS[g->anim_style]);
    render_text(g, g->font_small, line, g->panel_x, g->win_h - 268,
                (SDL_Color){ 150, 200, 215, 255 });
    render_text(g, g->font_small, "Ctrl+B board   Ctrl+P pieces   Ctrl+M anim",
                g->panel_x, g->win_h - 250, (SDL_Color){ 120, 140, 150, 255 });
}

static void render_setup(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_ui, "Singleplayer", g->win_w / 2, 150,
                         (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect sides[2], levels[3], start, back;
    setup_rects(g, sides, levels, &start, &back);

    render_text_centered(g, g->font_small, "Play as", g->win_w / 2, 268,
                         (SDL_Color){ 150, 180, 200, 255 });
    const char *side_labels[2] = { "White", "Black" };
    for (int i = 0; i < 2; i++) {
        bool chosen = (g->setup_side == i);
        bool focus  = chosen && g->setup_field == 0;
        draw_rect(ren, &sides[i], chosen ? 70 : 40, chosen ? 80 : 50,
                  chosen ? 100 : 60, true);
        if (chosen)
            draw_rect(ren, &sides[i], focus ? 90 : 65, focus ? 150 : 105,
                      focus ? 200 : 140, false);
        render_text_centered(g, g->font_ui, side_labels[i],
                             sides[i].x + sides[i].w / 2, sides[i].y + 12,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    render_text_centered(g, g->font_small, "Difficulty", g->win_w / 2, 388,
                         (SDL_Color){ 150, 180, 200, 255 });
    for (int i = 0; i < AI_LEVEL_COUNT; i++) {
        bool chosen = (g->setup_level == i);
        bool focus  = chosen && g->setup_field == 1;
        draw_rect(ren, &levels[i], chosen ? 70 : 40, chosen ? 80 : 50,
                  chosen ? 100 : 60, true);
        if (chosen)
            draw_rect(ren, &levels[i], focus ? 90 : 65, focus ? 150 : 105,
                      focus ? 200 : 140, false);
        render_text_centered(g, g->font_ui, AI_LEVELS[i].label,
                             levels[i].x + levels[i].w / 2, levels[i].y + 12,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    bool engine_ok = g->engine_path[0] != '\0';
    draw_rect(ren, &start, engine_ok ? 55 : 42, engine_ok ? 90 : 42, engine_ok ? 60 : 48, true);
    draw_rect(ren, &start, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, "Start", start.x + start.w / 2, start.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });

    draw_rect(ren, &back, 45, 45, 50, true);
    draw_rect(ren, &back, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, "Back", back.x + back.w / 2, back.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });

    if (!engine_ok)
        render_text_centered(g, g->font_small,
                             "Stockfish not found - run 'brew install stockfish'",
                             g->win_w / 2, 610, (SDL_Color){ 255, 140, 120, 255 });
    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, g->win_w / 2, 640,
                             (SDL_Color){ 255, 170, 120, 255 });

    render_text_centered(g, g->font_small,
                         "Left/Right change    Up/Down field    Enter start    Esc back",
                         g->win_w / 2, g->win_h - 80,
                         (SDL_Color){ 110, 120, 130, 255 });
}

static void hj_field(Gui *g, SDL_Renderer *ren, SDL_Rect *r,
                     const char *text, bool focused, const char *label)
{
    render_text(g, g->font_small, label, r->x, r->y - 20,
                (SDL_Color){ 150, 180, 200, 255 });
    draw_rect(ren, r, 30, 30, 32, true);
    if (focused) draw_rect(ren, r, 90, 140, 200, false);
    else         draw_rect(ren, r, 60, 60, 70, false);
    render_text(g, g->font_ui, text, r->x + 10, r->y + 8,
                (SDL_Color){ 235, 235, 235, 255 });
}

static void render_hostjoin(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_ui, "Local Multiplayer", g->win_w / 2, 150,
                         (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect addr, port, host, join, back;
    hj_rects(g, &addr, &port, &host, &join, &back);

    hj_field(g, ren, &addr, g->net_addr, g->hj_focus == 0, "Opponent address");
    hj_field(g, ren, &port, g->net_port, g->hj_focus == 1, "Port");

    draw_rect(ren, &host, g->hj_focus == 2 ? 70 : 45, g->hj_focus == 2 ? 85 : 50,
              g->hj_focus == 2 ? 105 : 60, true);
    if (g->hj_focus == 2) draw_rect(ren, &host, 90, 150, 200, false);
    render_text_centered(g, g->font_ui, "Host", host.x + host.w / 2, host.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });

    draw_rect(ren, &join, g->hj_focus == 3 ? 70 : 45, g->hj_focus == 3 ? 85 : 50,
              g->hj_focus == 3 ? 105 : 60, true);
    if (g->hj_focus == 3) draw_rect(ren, &join, 90, 150, 200, false);
    render_text_centered(g, g->font_ui, "Join", join.x + join.w / 2, join.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });

    draw_rect(ren, &back, g->hj_focus == 4 ? 70 : 45, g->hj_focus == 4 ? 85 : 50,
              g->hj_focus == 4 ? 105 : 60, true);
    if (g->hj_focus == 4) draw_rect(ren, &back, 90, 150, 200, false);
    render_text_centered(g, g->font_ui, "Back", back.x + back.w / 2, back.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });

    render_text_centered(g, g->font_small,
                         "Host = White, Join = Black.  Start the host first.",
                         g->win_w / 2, 560, (SDL_Color){ 140, 170, 190, 255 });

    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, g->win_w / 2, 600,
                             (SDL_Color){ 255, 180, 120, 255 });

    render_text_centered(g, g->font_small,
                         "Tab/Up/Down field    type to edit    Enter activate    Esc back",
                         g->win_w / 2, g->win_h - 80,
                         (SDL_Color){ 110, 120, 130, 255 });
}

/* ---- appearance thumbnails ---- */

static int thumb_native(Gui *g)
{
    int px = (int)lroundf(110.0f * g->ui_scale);
    return px > 0 ? px : 110;
}

static SDL_Texture *make_scaled_tex(Gui *g, const char *path, int px)
{
    SDL_Surface *src = IMG_Load(path);
    if (!src) return NULL;
    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, px, px, 32, SDL_PIXELFORMAT_RGBA32);
    if (!dst) { SDL_FreeSurface(src); return NULL; }

    SDL_Rect r = { 0, 0, px, px };
    if (src->w > 0 && src->h > 0) {
        int m = src->w > src->h ? src->w : src->h;
        float s = (float)px / (float)m;
        int w = (int)(src->w * s), h = (int)(src->h * s);
        r.x = (px - w) / 2; r.y = (px - h) / 2; r.w = w; r.h = h;
    }
    SDL_BlitScaled(src, NULL, dst, &r);
    SDL_Texture *t = SDL_CreateTextureFromSurface(g->ren, dst);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(src);
    SDL_FreeSurface(dst);
    return t;
}

/* Composite the set's white and black king onto one neutral thumbnail. */
static SDL_Texture *make_piece_thumb(Gui *g, const char *dir, int px)
{
    char wp[THEME_PATH_MAX + 16], bp[THEME_PATH_MAX + 16];
    snprintf(wp, sizeof wp, "%s/wk.png", dir);
    snprintf(bp, sizeof bp, "%s/bk.png", dir);
    SDL_Surface *w = IMG_Load(wp);
    SDL_Surface *b = IMG_Load(bp);
    if (!w || !b) {
        if (w) SDL_FreeSurface(w);
        if (b) SDL_FreeSurface(b);
        return NULL;
    }
    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, px, px, 32, SDL_PIXELFORMAT_RGBA32);
    if (!dst) { SDL_FreeSurface(w); SDL_FreeSurface(b); return NULL; }

    SDL_FillRect(dst, NULL, SDL_MapRGBA(dst->format, 96, 100, 110, 255));
    SDL_Rect l = { px / 8, px / 8, px * 3 / 8, px * 3 / 4 };
    SDL_Rect rr = { px / 2, px / 8, px * 3 / 8, px * 3 / 4 };
    SDL_BlitScaled(w, NULL, dst, &l);
    SDL_BlitScaled(b, NULL, dst, &rr);

    SDL_Texture *t = SDL_CreateTextureFromSurface(g->ren, dst);
    if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(w);
    SDL_FreeSurface(b);
    SDL_FreeSurface(dst);
    return t;
}

static SDL_Texture *board_thumb(Gui *g, int idx)
{
    if (!g->board_thumbs)
        g->board_thumbs = calloc((size_t)g->boards.count, sizeof(SDL_Texture *));
    if (!g->board_thumbs || idx < 0 || idx >= g->boards.count) return NULL;
    if (!g->board_thumbs[idx] && g->boards.items[idx].path[0])
        g->board_thumbs[idx] = make_scaled_tex(g, g->boards.items[idx].path, thumb_native(g));
    return g->board_thumbs[idx];
}

static SDL_Texture *piece_thumb(Gui *g, int idx)
{
    if (!g->piece_thumbs)
        g->piece_thumbs = calloc((size_t)g->pieces.count, sizeof(SDL_Texture *));
    if (!g->piece_thumbs || idx < 0 || idx >= g->pieces.count) return NULL;
    if (!g->piece_thumbs[idx] && g->pieces.items[idx].path[0])
        g->piece_thumbs[idx] = make_piece_thumb(g, g->pieces.items[idx].path, thumb_native(g));
    return g->piece_thumbs[idx];
}

static void draw_ape_cell(Gui *g, SDL_Renderer *ren, int tab, int idx,
                          SDL_Rect *cell, bool selected)
{
    bool active = (tab == 0) ? (idx == g->board_index)
                : (tab == 1) ? (idx == g->piece_index)
                             : (idx == (int)g->anim_style);

    draw_rect(ren, cell, selected ? 60 : 36, selected ? 70 : 40,
              selected ? 85 : 48, true);
    if (selected)   draw_rect(ren, cell, 90, 150, 200, false);
    else if (active) draw_rect(ren, cell, 90, 190, 120, false);

    int side = 110;
    SDL_Rect inner = { cell->x + (cell->w - side) / 2, cell->y + 8, side, side };

    if (tab == 0) {
        SDL_Texture *t = board_thumb(g, idx);
        if (t) {
            SDL_RenderCopy(ren, t, NULL, &inner);
        } else {
            ThemeEntry *e = &g->boards.items[idx];
            for (int r = 0; r < 8; r++)
                for (int f = 0; f < 8; f++) {
                    SDL_Rect q = { inner.x + f * side / 8, inner.y + r * side / 8,
                                   side / 8 + 1, side / 8 + 1 };
                    if ((r + f) % 2 == 0)
                        set_render_color(ren, e->light[0], e->light[1], e->light[2]);
                    else
                        set_render_color(ren, e->dark[0], e->dark[1], e->dark[2]);
                    SDL_RenderFillRect(ren, &q);
                }
        }
    } else if (tab == 1) {
        SDL_Texture *t = piece_thumb(g, idx);
        if (t) {
            SDL_RenderCopy(ren, t, NULL, &inner);
        } else {
            render_text_centered(g, g->font_piece[0], "\xE2\x99\x94",
                                 inner.x + side / 2, inner.y + 8,
                                 (SDL_Color){ 235, 235, 235, 255 });
        }
    } else {
        for (int r = 0; r < 8; r++)
            for (int f = 0; f < 8; f++) {
                SDL_Rect q = { inner.x + f * side / 8, inner.y + r * side / 8,
                               side / 8 + 1, side / 8 + 1 };
                if ((r + f) % 2 == 0) set_render_color(ren, 180, 180, 186);
                else                  set_render_color(ren, 110, 112, 120);
                SDL_RenderFillRect(ren, &q);
            }
        float t = (float)((SDL_GetTicks() - g->ape_started) % 1400) / 1400.0f;
        float fx = inner.x + side * 0.25f, fy = inner.y + side * 0.75f;
        float tx = inner.x + side * 0.75f, ty = inner.y + side * 0.25f;
        float x, y, sc, al;
        anim_piece_transform(g, (AnimStyle)idx, t, fx, fy, tx, ty, &x, &y, &sc, &al);
        render_piece_box(g, ren, WN, x, y, side * 0.34f * sc, al);
    }

    const char *label = (tab == 2) ? ANIM_LABELS[idx]
                      : (tab == 0) ? g->boards.items[idx].label
                                   : g->pieces.items[idx].label;
    render_text_centered(g, g->font_small, label, cell->x + cell->w / 2,
                         cell->y + cell->h - 24, (SDL_Color){ 210, 215, 225, 255 });
}

static void render_appearance(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_ui, "Appearance", g->win_w / 2, 30,
                         (SDL_Color){ 235, 225, 200, 255 });

    static const char *tabs[3] = { "Boards", "Pieces", "Animation" };
    for (int i = 0; i < 3; i++) {
        SDL_Rect r;
        ape_tab_rect(g, i, &r);
        bool on = (i == g->ape_tab);
        draw_rect(ren, &r, on ? 70 : 40, on ? 80 : 50, on ? 100 : 60, true);
        if (on) draw_rect(ren, &r, 90, 150, 200, false);
        render_text_centered(g, g->font_ui, tabs[i], r.x + r.w / 2, r.y + 9,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    int tab = g->ape_tab;
    int n = appearance_count(g, tab);
    int sel = g->ape_sel[tab];
    int page = n > 0 ? sel / APE_PER_PAGE : 0;
    int start = page * APE_PER_PAGE;
    for (int j = 0; j < APE_PER_PAGE; j++) {
        int idx = start + j;
        if (idx >= n) break;
        SDL_Rect c;
        ape_cell_rect(g, j, &c);
        draw_ape_cell(g, ren, tab, idx, &c, idx == sel);
    }

    int pages = (n + APE_PER_PAGE - 1) / APE_PER_PAGE;
    if (pages < 1) pages = 1;
    char info[64];
    snprintf(info, sizeof info, "%d / %d    page %d/%d", sel + 1, n, page + 1, pages);
    render_text_centered(g, g->font_small, info, g->win_w / 2, 662,
                         (SDL_Color){ 150, 170, 190, 255 });

    SDL_Rect back;
    ape_back_rect(g, &back);
    render_text_centered_rect(g, ren, &back, "Back");

    render_text_centered(g, g->font_small,
                         "Arrows select    Tab / 1-2-3 switch tab    Enter apply    Esc back",
                         g->win_w / 2, g->win_h - 40,
                         (SDL_Color){ 110, 120, 130, 255 });
}

/* ---- Settings panel ------------------------------------------------ */

#define SET_TABS 3
static const char *SET_TAB_LABELS[SET_TABS] = { "Engine", "Gameplay", "Audio & Video" };

static void settings_tab_rect(const Gui *g, int i, SDL_Rect *r)
{
    int w = 190, h = 34, gap = 12;
    int total = SET_TABS * w + (SET_TABS - 1) * gap;
    int x0 = (g->win_w - total) / 2;
    *r = (SDL_Rect){ x0 + i * (w + gap), 74, w, h };
}

/* Gameplay tab rows */
enum { GP_BOARD = 0, GP_PIECES, GP_ANIM, GP_APPEARANCE, GP_COUNT };
static const char *GP_LABELS[GP_COUNT] = {
    "Board", "Pieces", "Animation", "Appearance picker"
};

/* Audio & Video tab rows */
enum { AV_SOUND = 0, AV_FPS, AV_COUNT };
static const char *AV_LABELS[AV_COUNT] = { "Sound", "Max FPS" };

static const int FPS_CHOICES[] = { 0, 60, 120, 144, 240, 320 };
#define FPS_CHOICE_COUNT ((int)(sizeof FPS_CHOICES / sizeof FPS_CHOICES[0]))

static void settings_row_rect(const Gui *g, int i, SDL_Rect *r)
{
    int h = 40, gap = 14;
    *r = (SDL_Rect){ 120, 180 + i * (h + gap), g->win_w - 240, h };
}

static void settings_back_rect(const Gui *g, SDL_Rect *r)
{
    r->w = 140; r->h = 42; r->x = g->win_w - 160; r->y = g->win_h - 90;
}

static void settings_gp_value(const Gui *g, int row, char *out, size_t n)
{
    switch (row) {
        case GP_BOARD:  snprintf(out, n, "%s", theme_label(&g->boards, g->board_index)); break;
        case GP_PIECES: snprintf(out, n, "%s", theme_label(&g->pieces, g->piece_index)); break;
        case GP_ANIM:   snprintf(out, n, "%s", ANIM_LABELS[g->anim_style]); break;
        default:        snprintf(out, n, "Open >"); break;
    }
}

static void settings_av_value(const Gui *g, int row, char *out, size_t n)
{
    if (row == AV_SOUND) {
        snprintf(out, n, "%s", g->sound ? "On" : "Off");
    } else if (g->max_fps == 0) {
        snprintf(out, n, "vsync");
    } else {
        snprintf(out, n, "%d", g->max_fps);
    }
}

static void settings_row_adjust(Gui *g, int tab, int row, int dir)
{
    if (tab == 1) {
        switch (row) {
            case GP_BOARD:
                if (g->boards.count > 0) {
                    g->board_index = (g->board_index + dir + g->boards.count) % g->boards.count;
                    load_board_style(g);
                }
                break;
            case GP_PIECES:
                if (g->pieces.count > 0) {
                    g->piece_index = (g->piece_index + dir + g->pieces.count) % g->pieces.count;
                    load_piece_style(g);
                }
                break;
            case GP_ANIM:
                g->anim_style = (AnimStyle)((g->anim_style + dir + ANIM_STYLE_COUNT) % ANIM_STYLE_COUNT);
                clear_anims(g);
                break;
            default: return;
        }
    } else {
        if (row == AV_SOUND) {
            g->sound = !g->sound;
            audio_set_enabled(g->sound);
            if (g->sound) audio_play(SND_NOTIFY);
        } else {
            int idx = 0;
            for (int i = 0; i < FPS_CHOICE_COUNT; i++)
                if (FPS_CHOICES[i] == g->max_fps) idx = i;
            idx = (idx + dir + FPS_CHOICE_COUNT) % FPS_CHOICE_COUNT;
            g->max_fps = FPS_CHOICES[idx];
            gui_apply_vsync(g);
        }
    }
    g->config_dirty = true;
}

static void settings_row_activate(Gui *g, int tab, int row)
{
    if (tab == 1 && row == GP_APPEARANCE) open_appearance(g, SCENE_SETTINGS);
    else settings_row_adjust(g, tab, row, +1);
}

static void handle_settings_engine_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    handle_engine_keydown(g, ke);
}

static void handle_settings_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE) { g->scene = g->engine_return_scene; return; }

    if (k == SDLK_TAB || k == SDLK_1 || k == SDLK_2 || k == SDLK_3) {
        if (k == SDLK_TAB) g->settings_tab = (g->settings_tab + 1) % SET_TABS;
        else               g->settings_tab = (k == SDLK_1) ? 0 : (k == SDLK_2) ? 1 : 2;
        g->settings_row = 0;
        g->engine_custom_focus = false;
        g->eng_ctrl_focus = -1;
        return;
    }

    if (g->settings_tab == 0) { handle_settings_engine_keydown(g, ke); return; }

    int n = (g->settings_tab == 1) ? GP_COUNT : AV_COUNT;
    if (k == SDLK_UP)   { if (g->settings_row > 0) g->settings_row--; return; }
    if (k == SDLK_DOWN) { if (g->settings_row + 1 < n) g->settings_row++; return; }
    if (k == SDLK_LEFT)  { settings_row_adjust(g, g->settings_tab, g->settings_row, -1); return; }
    if (k == SDLK_RIGHT) { settings_row_adjust(g, g->settings_tab, g->settings_row, +1); return; }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER)
        settings_row_activate(g, g->settings_tab, g->settings_row);
}

static void handle_settings_mousedown(Gui *g)
{
    SDL_Point p = g->mouse;

    for (int i = 0; i < SET_TABS; i++) {
        SDL_Rect r;
        settings_tab_rect(g, i, &r);
        if (pt_in(&r, p.x, p.y)) {
            g->settings_tab = i;
            g->settings_row = 0;
            g->engine_custom_focus = false;
            g->eng_ctrl_focus = -1;
            return;
        }
    }

    SDL_Rect back;
    settings_back_rect(g, &back);
    if (pt_in(&back, p.x, p.y)) { g->scene = g->engine_return_scene; return; }

    if (g->settings_tab == 0) { handle_engine_mousedown(g); return; }

    int n = (g->settings_tab == 1) ? GP_COUNT : AV_COUNT;
    for (int i = 0; i < n; i++) {
        SDL_Rect r;
        settings_row_rect(g, i, &r);
        SDL_Rect minus = { r.x + r.w - 90, r.y + 5, 30, 30 };
        SDL_Rect plus  = { r.x + r.w - 50, r.y + 5, 30, 30 };
        if (pt_in(&minus, p.x, p.y)) { g->settings_row = i; settings_row_adjust(g, g->settings_tab, i, -1); return; }
        if (pt_in(&plus, p.x, p.y))  { g->settings_row = i; settings_row_adjust(g, g->settings_tab, i, +1); return; }
        if (pt_in(&r, p.x, p.y))     { g->settings_row = i; settings_row_activate(g, g->settings_tab, i); return; }
    }
}

static void render_settings_tabs(Gui *g, SDL_Renderer *ren)
{
    for (int i = 0; i < SET_TABS; i++) {
        SDL_Rect r;
        settings_tab_rect(g, i, &r);
        bool on = (i == g->settings_tab);
        draw_rect(ren, &r, on ? 60 : 34, on ? 80 : 40, on ? 110 : 48, true);
        if (on) draw_rect(ren, &r, 90, 150, 200, false);
        render_text_centered(g, g->font_small, SET_TAB_LABELS[i],
                             r.x + r.w / 2, r.y + 9, (SDL_Color){ 230, 230, 230, 255 });
    }
}

static void render_settings_engine(Gui *g, SDL_Renderer *ren)
{
    char cur[600];
    snprintf(cur, sizeof cur, "Current: %s",
             g->engine_path[0] ? g->engine_path : "(none)");
    render_text(g, g->font_small, cur, 80, 126, (SDL_Color){ 150, 180, 200, 255 });

    SDL_Rect items[ENGINE_MAX], custom, back;
    engine_rects(g, items, &custom, &back);

    for (int i = 0; i < g->engine_count; i++) {
        bool sel = (i == g->engine_sel);
        draw_rect(ren, &items[i], sel ? 60 : 36, sel ? 70 : 40, sel ? 85 : 48, true);
        if (sel) draw_rect(ren, &items[i], 90, 150, 200, false);
        render_text(g, g->font_small, g->engine_candidates[i],
                    items[i].x + 10, items[i].y + 10, (SDL_Color){ 225, 230, 235, 255 });
    }
    if (g->engine_count == 0)
        render_text_centered(g, g->font_small,
                             "No engines found on PATH - enter a path below",
                             g->win_w / 2, 190, (SDL_Color){ 200, 150, 140, 255 });

    render_text(g, g->font_small, "Analysis settings", 80, 150,
                (SDL_Color){ 200, 200, 200, 255 });
    SDL_Rect minus[ENG_CTRL_COUNT], plus[ENG_CTRL_COUNT];
    engine_ctrl_rects(g, minus, plus);
    for (int i = 0; i < ENG_CTRL_COUNT; i++) {
        int y = minus[i].y;
        bool on = (i == g->eng_ctrl_focus);
        render_text(g, g->font_small, ENG_CTRL_LABELS[i], 80, y + 6,
                    on ? (SDL_Color){ 235, 235, 235, 255 }
                       : (SDL_Color){ 175, 185, 195, 255 });
        char val[32];
        int v = eng_ctrl_value(g, i);
        if ((i == 3 || i == 4) && v == 0) snprintf(val, sizeof val, "unlimited");
        else if (i == 0 && v == 0)        snprintf(val, sizeof val, "off");
        else                              snprintf(val, sizeof val, "%d", v);
        render_text(g, g->font_small, val, 158, y + 6, (SDL_Color){ 220, 225, 235, 255 });
        draw_rect(ren, &minus[i], on ? 90 : 55, on ? 150 : 60, on ? 200 : 75, true);
        render_text_centered(g, g->font_small, "-", minus[i].x + minus[i].w / 2, y + 6,
                             (SDL_Color){ 235, 235, 235, 255 });
        draw_rect(ren, &plus[i], on ? 90 : 55, on ? 150 : 60, on ? 200 : 75, true);
        render_text_centered(g, g->font_small, "+", plus[i].x + plus[i].w / 2, y + 6,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    render_text(g, g->font_small, "Custom engine path (click to edit, Enter to apply):",
                custom.x, custom.y - 22, (SDL_Color){ 150, 180, 200, 255 });
    draw_rect(ren, &custom, 30, 30, 32, true);
    draw_rect(ren, &custom,
              g->engine_custom_focus ? 90 : 60,
              g->engine_custom_focus ? 140 : 60,
              g->engine_custom_focus ? 200 : 70, false);
    render_text(g, g->font_small, g->engine_custom, custom.x + 10, custom.y + 10,
                (SDL_Color){ 225, 230, 235, 255 });

    if (g->engine_status[0])
        render_text_centered(g, g->font_small, g->engine_status,
                             g->win_w / 2, custom.y + 46,
                             (SDL_Color){ 150, 210, 170, 255 });
}

static void render_settings_rows(Gui *g, SDL_Renderer *ren)
{
    int n = (g->settings_tab == 1) ? GP_COUNT : AV_COUNT;
    for (int i = 0; i < n; i++) {
        SDL_Rect r;
        settings_row_rect(g, i, &r);
        bool on = (i == g->settings_row);
        const char *label = (g->settings_tab == 1) ? GP_LABELS[i] : AV_LABELS[i];
        draw_rect(ren, &r, on ? 52 : 36, on ? 66 : 42, on ? 86 : 50, true);
        if (on) draw_rect(ren, &r, 90, 150, 200, false);
        int ty = r.y + (r.h - text_height(g, g->font_ui)) / 2;
        render_text(g, g->font_ui, label, r.x + 16, ty,
                    (SDL_Color){ 225, 230, 235, 255 });

        char val[64];
        if (g->settings_tab == 1) settings_gp_value(g, i, val, sizeof val);
        else                      settings_av_value(g, i, val, sizeof val);
        render_text_centered(g, g->font_ui, val, r.x + r.w - 160, ty,
                             (SDL_Color){ 200, 215, 225, 255 });

        if (g->settings_tab == 1 && i == GP_APPEARANCE) continue;
        SDL_Rect minus = { r.x + r.w - 90, r.y + 5, 30, 30 };
        SDL_Rect plus  = { r.x + r.w - 50, r.y + 5, 30, 30 };
        draw_rect(ren, &minus, 55, 60, 75, true);
        render_text_centered(g, g->font_small, "-", minus.x + minus.w / 2, minus.y + 6,
                             (SDL_Color){ 235, 235, 235, 255 });
        draw_rect(ren, &plus, 55, 60, 75, true);
        render_text_centered(g, g->font_small, "+", plus.x + plus.w / 2, plus.y + 6,
                             (SDL_Color){ 235, 235, 235, 255 });
    }
}

static void render_settings(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_ui, "Settings", g->win_w / 2, 28,
                         (SDL_Color){ 235, 225, 200, 255 });
    render_settings_tabs(g, ren);

    if (g->settings_tab == 0) render_settings_engine(g, ren);
    else                      render_settings_rows(g, ren);

    SDL_Rect back;
    settings_back_rect(g, &back);
    render_text_centered_rect(g, ren, &back, "Back");

    render_text_centered(g, g->font_small,
        "Tab / 1-2-3 switch tab    Up/Down select    Left/Right change    Enter open    Esc back",
        g->win_w / 2, g->win_h - 40, (SDL_Color){ 110, 120, 130, 255 });
}

static void render_menu(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_piece[0], "Chess", g->win_w / 2, 40,
                         (SDL_Color){ 235, 225, 200, 255 });
    render_text_centered(g, g->font_small, "C / SDL chess board", g->win_w / 2,
                         40 + text_height(g, g->font_piece[0]) + 10,
                         (SDL_Color){ 130, 170, 190, 255 });

    int ty_off = (MENU_ITEM_H - text_height(g, g->font_ui)) / 2;

    for (int i = 0; i < menu_count(g); i++) {
        SDL_Rect r;
        menu_item_rect(g, i, &r);
        bool sel = (i == g->menu_index);
        bool en = menu_item_enabled(g, i);

        if (en) {
            Uint8 base = sel ? 70 : 40;
            set_render_color(ren, base, base + 10, base + 20);
            SDL_RenderFillRect(ren, &r);
            if (sel) draw_rect(ren, &r, 90, 150, 200, false);
        }

        SDL_Color tc = !en ? (SDL_Color){ 100, 105, 115, 255 }
                      : sel ? (SDL_Color){ 245, 245, 245, 255 }
                            : (SDL_Color){ 205, 210, 220, 255 };
        render_text_centered(g, g->font_ui, menu_label(g, i), g->win_w / 2,
                             r.y + ty_off, tc);
    }

    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, g->win_w / 2,
                             g->win_h - 118, (SDL_Color){ 255, 170, 120, 255 });

    render_text_centered(g, g->font_small,
                         "Up/Down select    Enter choose    Ctrl+Q quit",
                         g->win_w / 2, g->win_h - 70,
                         (SDL_Color){ 110, 120, 130, 255 });
}

static void render_pgn_prompt(Gui *g, SDL_Renderer *ren)
{
    if (!g->pgn_prompt) return;

    int bw = 460, bh = 150;
    SDL_Rect box = { (g->win_w - bw) / 2, (g->win_h - bh) / 2, bw, bh };
    draw_rect(ren, &box, 40, 44, 52, true);
    draw_rect(ren, &box, 120, 130, 150, false);

    render_text(g, g->font_ui, "Save PGN as:", box.x + 20, box.y + 16,
                (SDL_Color){ 235, 235, 235, 255 });

    SDL_Rect field = { box.x + 20, box.y + 52, bw - 40, 40 };
    draw_rect(ren, &field, 25, 25, 30, true);
    draw_rect(ren, &field, 90, 140, 200, false);

    const char *shown = g->pgn_len ? g->pgn_name : "game";
    SDL_Color tc = g->pgn_len ? (SDL_Color){ 235, 235, 235, 255 }
                              : (SDL_Color){ 110, 110, 115, 255 };
    SDL_RenderSetClipRect(ren, &field);
    render_text(g, g->font_ui, shown, field.x + 8, field.y + 8, tc);
    if (g->pgn_len) {
        int w = 0, h = 0;
        TTF_SizeUTF8(g->font_ui, g->pgn_name, &w, &h);
        float sc = eff_scale(g);
        int lw = (int)lroundf(w / sc);
        set_render_color(ren, 240, 240, 240);
        SDL_Rect caret = { field.x + 10 + lw, field.y + 8, 2, field.h - 16 };
        SDL_RenderFillRect(ren, &caret);
    }
    SDL_RenderSetClipRect(ren, NULL);

    render_text_centered(g, g->font_small, "Enter save    Esc cancel",
                         box.x + bw / 2, box.y + bh - 26,
                         (SDL_Color){ 150, 160, 175, 255 });
}

static void render_game(Gui *g, SDL_Renderer *ren, Uint32 now)
{
    set_render_color(ren, 30, 30, 34);
    SDL_RenderClear(ren);

    draw_board_squares(g, ren);
    draw_coordinates(g);
    draw_eval_bar(g, ren);

    if (g->ply > 0) {
        Move m = g->history[g->ply - 1];
        draw_square_highlight(g, ren, MOVE_FROM(m), 215, 215, 70);
        draw_square_highlight(g, ren, MOVE_TO(m), 215, 215, 70);
    }

    if (in_check(&g->board, g->board.side) && g->state == NO_GAME_OVER) {
        int k = find_king(&g->board, g->board.side);
        if (k >= 0) draw_square_highlight(g, ren, k, 220, 70, 70);
    }

    if (g->selected >= 0) draw_square_highlight(g, ren, g->selected, 120, 190, 120);

    if (g->selected >= 0)
        for (int i = 0; i < g->targets.count; i++) {
            Move m = g->targets.moves[i];
            bool cap = (MOVE_FLAGS(m) & (FLAG_CAPTURE | FLAG_EP)) ? true : false;
            if (MOVE_FLAGS(m) & FLAG_PROMO) cap = true;
            draw_target_dot(g, ren, MOVE_TO(m), cap);
        }

    draw_piece_layer(g, ren, now);
    draw_engine_arrows(g, ren);
    draw_annotations(g, ren);

    /* Board resize grip (three diagonal ticks in the bottom-right corner). */
    {
        SDL_Rect grip;
        board_grip_rect(g, &grip);
        set_render_color(ren, 150, 155, 165);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (int i = 0; i < 3; i++) {
            SDL_RenderDrawLine(ren,
                               grip.x + grip.w - 3 - i * 5, grip.y + grip.h - 1,
                               grip.x + grip.w - 1, grip.y + grip.h - 3 - i * 5);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    if (g->promo_from >= 0) draw_promo_chooser(g, ren);

    if (g->dragging && g->selected >= 0) {
        render_piece_box(g, ren, g->board.board[g->selected],
                         (float)g->mouse.x, (float)g->mouse.y, g->sq + 18, 1.0f);
    }

    render_status(g);
    render_engine_panel(g, ren);
    render_move_list(g);
    if (g->mode == MODE_ANALYSIS) render_fen_box(g, ren);
    render_hud(g);
    if (g->san_open) render_input_box(g, ren);
    else             render_san_hint(g);
    render_buttons(g, ren);

    if (g->msg[0] && SDL_GetTicks() < g->msg_until) {
        render_text(g, g->font_small, g->msg, g->panel_x, g->win_h - 150,
                    (SDL_Color){ 255, 160, 120, 255 });
    }

    render_pgn_prompt(g, ren);
}

/* Diagnostic overlay (OPENCHESS_DEBUG_UI=1): shows where the app maps the
 * cursor vs. where SDL's own forward conversion says that base point lands.
 * Cyan = app mapping (set_mouse), yellow = SDL_RenderLogicalToWindow. */
static void draw_debug_overlay(Gui *g, SDL_Renderer *ren)
{
    if (!g->debug_ui || !g->ren) return;

    float cx = (float)g->mouse.x, cy = (float)g->mouse.y;
    float yx = cx, yy = cy;
    int sdlwx = g->mouse_win.x, sdly = g->mouse_win.y;
    float s  = g->tf_scale > 0.0f ? g->tf_scale : 1.0f;
    float ux = g->tf_ux > 0.0f ? g->tf_ux : 1.0f;
    float uy = g->tf_uy > 0.0f ? g->tf_uy : 1.0f;

#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_RenderLogicalToWindow(g->ren, cx, cy, &sdlwx, &sdly);
    yx = (float)sdlwx * ux / s - g->tf_vpx;
    yy = (float)sdly * uy / s - g->tf_vpy;
#endif
    /* magenta = raw event coords mapped (what input would use without the fix) */
    float exb = (float)g->mouse_evt.x * ux / s - g->tf_vpx;
    float eyb = (float)g->mouse_evt.y * uy / s - g->tf_vpy;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    /* square the cursor maps to (app) */
    int sq = sq_from_pos(g, g->mouse.x, g->mouse.y);
    if (sq >= 0) {
        SDL_Rect rc;
        window_sq(g, sq, &rc);
        set_render_color(ren, 80, 230, 240);
        SDL_RenderDrawRect(ren, &rc);
    }

    /* crosshairs */
    set_render_color(ren, 80, 230, 240);   /* cyan = app inverse */
    SDL_RenderDrawLine(ren, (int)cx - 9, (int)cy, (int)cx + 9, (int)cy);
    SDL_RenderDrawLine(ren, (int)cx, (int)cy - 9, (int)cx, (int)cy + 9);

    set_render_color(ren, 250, 220, 60);   /* yellow = SDL forward */
    SDL_RenderDrawLine(ren, (int)yx - 12, (int)yy, (int)yx + 12, (int)yy);
    SDL_RenderDrawLine(ren, (int)yx, (int)yy - 12, (int)yx, (int)yy + 12);

    set_render_color(ren, 240, 80, 220);   /* magenta = raw event mapped */
    SDL_RenderDrawLine(ren, (int)exb - 15, (int)eyb, (int)exb + 15, (int)eyb);
    SDL_RenderDrawLine(ren, (int)exb, (int)eyb - 15, (int)exb, (int)eyb + 15);

    /* readout */
    char buf[320];
    snprintf(buf, sizeof buf,
             "evt=(%d,%d) -> win=(%d,%d) base=(%d,%d)  global=(%d,%d) winpos=(%d,%d)\n"
             "sdl_fwd=(%d,%d) scale=%.3f vp=%.1f,%.1f u=%.2f,%.2f zoom=%.3f ui=%.3f pan=%.1f,%.1f\n"
             "cyan=app  yellow=SDL  magenta=raw-event",
             g->mouse_evt.x, g->mouse_evt.y, g->mouse_win.x, g->mouse_win.y,
             g->mouse.x, g->mouse.y, g->mouse_global.x, g->mouse_global.y,
             g->win_pos.x, g->win_pos.y,
             sdlwx, sdly, g->tf_scale, g->tf_vpx, g->tf_vpy,
             g->tf_ux, g->tf_uy, g->zoom, g->ui_scale, g->pan_x, g->pan_y);

    SDL_Rect bg = { g->board_x, g->board_y, 600, 76 };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 200);
    SDL_RenderFillRect(ren, &bg);
    render_text(g, g->font_small, buf, g->board_x + 6, g->board_y + 4,
                (SDL_Color){ 255, 255, 255, 255 });

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

void gui_render(Gui *g, SDL_Renderer *ren)
{
    Uint32 now = SDL_GetTicks();
    gui_anim_advance(g, now);

    /* Keep the drawing scale in sync with the intended magnification even if
     * SDL reset it (e.g. after a window/display change). */
    apply_render_scale(g);

    if (g->scene == SCENE_MENU)
        render_menu(g, ren);
    else if (g->scene == SCENE_SINGLE_SETUP)
        render_setup(g, ren);
    else if (g->scene == SCENE_HOSTJOIN)
        render_hostjoin(g, ren);
    else if (g->scene == SCENE_APPEARANCE)
        render_appearance(g, ren);
    else if (g->scene == SCENE_SETTINGS)
        render_settings(g, ren);
    else
        render_game(g, ren, now);

    draw_debug_overlay(g, ren);
    SDL_RenderPresent(ren);
}
