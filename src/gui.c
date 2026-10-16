#include "gui.h"
#include "pgn.h"
#include "fen.h"
#include "paths.h"
#include "audio.h"
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <SDL_image.h>
#ifdef _WIN32
#include <io.h>
#endif

#define GUI_PI 3.14159265358979323846f

static void ann_color_for(Uint8 *r, Uint8 *g, Uint8 *b);
static void engine_slider_rect(const Gui *g, SDL_Rect *track);
static void engine_arrows_rect(const Gui *g, SDL_Rect *box);
static int  eng_slider_from_x(const Gui *g, int mx);
static int  clampi(int v, int lo, int hi);
static void fen_refresh(Gui *g);
static bool puzzle_human_move(Gui *g, Move m);
static void puzzle_mistake(Gui *g);
static void puzzle_rewind(Gui *g,int step);
static void puzzle_prepare_input(Gui *g);
static void history_seek(Gui *g,int ply);
static void history_navigate(Gui *g,int delta);
static bool history_board(Gui *g,Board *out);
static void ui_puzzle_feedback(Gui *g);
static void ui_result_icon(Gui *g,float x,float y,float radius,bool correct,bool disc);
static void ui_disabled_button(Gui *g,SDL_Rect rect,const char *label);
static bool ui_finished_game(Gui *g);
static void ui_game_analyse(Gui *g);
static void review_start(Gui *g);
static void review_step(Gui *g);
static void open_account(Gui *g);
static void account_submit(Gui *g);
static void account_rects(const Gui *g, SDL_Rect *box, SDL_Rect *user,
                          SDL_Rect *pass, SDL_Rect btn[3]);
static void render_account(Gui *g, SDL_Renderer *ren);
static void fill_circle(SDL_Renderer *ren, int cx, int cy, int rad);
static void draw_badge_circle(Gui *g, SDL_Renderer *ren, int cx, int cy, int rad,
                              ReviewClass c);
static void draw_chip(Gui *g, SDL_Renderer *ren, int x, int y, int w, int h,
                      const char *txt);
static int  mv_width(Gui *g, const char *s);
static float pick_scale(const Gui *g, Uint32 now, bool dragging);
static void ui_init(Gui *g);
static void ui_close(Gui *g);
static void ui_assets(Gui *g);
static void ui_background(Gui *g, SDL_Renderer *ren);
static void ui_home(Gui *g);
static void ui_tick(Gui *g, Uint32 now);
static bool ui_event(Gui *g,const SDL_Event *event);
static bool ui_render_page(Gui *g);
static void ui_shell(Gui *g);
static void ui_modal_render(Gui *g);
static void ui_puzzle_strip(Gui *g);
static void ui_daily_panel(Gui *g);
static void ui_next_puzzle(Gui *g,int day);
static void ui_puzzle_settle(Gui *g,bool solved);
static void ui_save_analysis(Gui *g,const char *name);
static const char *ui_old_text(Gui *g,const char *s);
static void ui_round(SDL_Renderer *r,SDL_Rect rect,SDL_Color color,int radius);
static void ui_button(Gui *g,SDL_Rect r,const char *label,int icon,bool primary);
static const char *ui_tr(Gui *g,const char *en,const char *zh);
static void ui_apply_theme(Gui *g);
static SDL_Color ui_surface_color(SDL_Color color);
static SDL_Color ui_legacy_text_color(Gui *g,TTF_Font *font,SDL_Color color);
static void ui_tooltip_render(Gui *g);
static void ui_round_outline(SDL_Renderer *r,SDL_Rect rect,SDL_Color color);
static void ui_panel_border(SDL_Renderer *r,SDL_Rect rect,int radius);
static void ui_move_background(Gui *g,SDL_Rect rect,bool selected);
static void ui_puzzle_rating(Gui *g);
static void ui_caret_color(SDL_Renderer *ren);
static bool ui_action_buttons(Gui *g,bool click);
static void ui_local_history(Gui *g);
static bool p2p_active(const Gui *g);
static bool p2p_finished(const Gui *g);
static void p2p_open(Gui *g);
static void p2p_tick(Gui *g,Uint32 now);
static void p2p_action(Gui *g);
static void p2p_draw(Gui *g,bool accept);
static bool p2p_event(Gui *g,const SDL_Event *e);
static void p2p_render(Gui *g);



/* ------------------------------------------------------------------ */
/* style registries                                                    */
/* ------------------------------------------------------------------ */

static const char *ANIM_KEYS[ANIM_STYLE_COUNT]   = { "arcade", "slide", "fade", "none" };
static const char *ANIM_LABELS[ANIM_STYLE_COUNT] = { "Arcade", "Slide", "Fade", "None" };
/* One cadence for click moves, playback, previews and reverse moves. */
static const Uint32 ANIM_DURS[ANIM_STYLE_COUNT]  = { 380, 380, 380, 0 };
#define PICK_GLOW_SIZE 256
#define PICK_GLOW_FRAMES 24
#define PICK_GLOW_COLS 6

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
    g->win_w   = WIN_W;
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
    g->eval_best_uci[0]=0;

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
    if (g->review_on) return;   /* don't restart the live eval during review */
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

static const char *const UI_FONT_NAMES[]={"Maple Mono NF CN","Noto Sans CJK"};
static const char *ui_font_file(int index,char path[1200])
{
    snprintf(path,1200,"%s/fonts/%s",path_assets(),index==0?"MapleMono-NF-CN-Regular.ttf":"NotoSansCJKsc-Regular.otf");
    FILE *probe=fopen(path,"rb");if(probe){fclose(probe);return path;}
    snprintf(path,1200,"%s/fonts/NotoSansCJKsc-Regular.otf",path_assets());
    probe=fopen(path,"rb");if(probe){fclose(probe);return path;}
    return find_font();
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
    if(r<110&&g<130&&b<150&&fill){r=242;g=248;b=253;}
    set_render_color(ren, r, g, b);
    if (fill) {
        ui_round(ren,*rc,(SDL_Color){r,g,b,255},6);
        if(rc->w>=120&&rc->h>=50)ui_panel_border(ren,*rc,6);
    }else if(abs((int)r-g)<24&&abs((int)g-b)<24)ui_panel_border(ren,*rc,6);
    else ui_round_outline(ren,*rc,(SDL_Color){r,g,b,255});
}

static void render_text(Gui *g, TTF_Font *font, const char *txt,
                        int x, int y, SDL_Color col)
{
    if (!font || !txt || !*txt) return;
    SDL_Renderer *ren = g->ren;
    txt=ui_old_text(g,txt);
    col=ui_legacy_text_color(g,font,col);
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, txt, col);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    float sc = eff_scale(g);
    SDL_Rect rc = { x, y, (int)lroundf(s->w / sc), (int)lroundf(s->h / sc) };
    SDL_RenderCopy(ren, t, NULL, &rc);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static void render_text_exact(Gui *g,TTF_Font *font,const char *text,int x,int y,SDL_Color color)
{
    bool legacy=g->ui&&g->ui->legacy_paint;if(g->ui)g->ui->legacy_paint=false;
    render_text(g,font,text,x,y,color);if(g->ui)g->ui->legacy_paint=legacy;
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
    txt=ui_old_text(g,txt);
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

    /* Input belongs to the event timestamp and target window. Querying the
     * current global pointer here races with queued clicks and other windows
     * (including synthetic input), producing clicks at a different location. */
    int x=ex,y=ey;
    SDL_GetGlobalMouseState(&g->mouse_global.x,&g->mouse_global.y);
    SDL_GetWindowPosition(g->win,&g->win_pos.x,&g->win_pos.y);
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
    for(int i=0;i<2;i++){if(g->font_preview[i])TTF_CloseFont(g->font_preview[i]);g->font_preview[i]=NULL;}
    if (g->font_ui) TTF_CloseFont(g->font_ui);
    if (g->font_small) TTF_CloseFont(g->font_small);
    if (g->font_tiny) TTF_CloseFont(g->font_tiny);
    if (g->font_rating) TTF_CloseFont(g->font_rating);
    if (g->font_move) TTF_CloseFont(g->font_move);
    g->font_piece[0] = g->font_piece[1] = NULL;
    g->font_ui = g->font_small = g->font_tiny = NULL;
    g->font_move = g->font_rating = NULL;

    const char *fpath = find_font();
    if (fpath) {
        char ui_font[1200];const char *ui_path=ui_font_file(g->font_index,ui_font);
        float sc = eff_scale(g);
        g->font_piece[0] = TTF_OpenFont(fpath, (int)lroundf(SQ_SIZE * sc));
        g->font_piece[1] = TTF_OpenFont(fpath, (int)lroundf(SQ_SIZE * sc));
        g->font_ui      = TTF_OpenFont(ui_path, (int)lroundf(22 * sc));
        g->font_small   = TTF_OpenFont(ui_path, (int)lroundf(15 * sc));
        g->font_tiny    = TTF_OpenFont(ui_path, (int)lroundf(12 * sc));
        g->font_move    = TTF_OpenFont(ui_path, (int)lroundf(17 * sc));
        g->font_rating  = TTF_OpenFont(ui_path, (int)lroundf(32 * sc));
        if(g->font_rating)TTF_SetFontStyle(g->font_rating,TTF_STYLE_BOLD);
        if (g->font_move) TTF_SetFontStyle(g->font_move, TTF_STYLE_BOLD);
        if (!g->font_piece[0] || !g->font_piece[1] || !g->font_ui ||
            !g->font_small || !g->font_tiny || !g->font_move)
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
    g->pick_started = 0;
    g->pick_released = 0;
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
    if (g->state != NO_GAME_OVER) return;
    if (g->board.halfmove_clock >= 100) { g->state = FIFTY_MOVE_RULE; return; }
    if (g->ply > 0 &&
        board_repetitions(&g->board, g->before, g->ply) + 1 >= 3)
        g->state = THREEFOLD_REPETITION;
}

/* True when the board still matches the current tree node (so tree ops apply). */
static bool tree_synced(const Gui *g)
{
    return g->pgntree && g->tree_cur &&
           board_rep_equal(&g->board, &g->tree_cur->board);
}

/* Make the linear arrays track the tree path root..tree_cur. */
static void sync_from_tree(Gui *g)
{
    g->history_browsing=false;
    MoveNode *chain[MAX_PLY];
    int d = 0;
    for (MoveNode *n = g->tree_cur; n && n->parent && d < MAX_PLY; n = n->parent)
        chain[d++] = n;

    for (int i = 0; i < d; i++) {
        MoveNode *n = chain[d - 1 - i];
        g->before[i] = n->parent->board;
        g->history[i] = n->move;
        snprintf(g->move_san[i], 8, "%s", n->san);
        g->path_nodes[i] = n;
    }
    g->path_len = d;
    g->ply = d;
    if (g->tree_cur) g->board = g->tree_cur->board;
    clear_selection(g);
    ann_clear(g);
    g->promo_from = g->promo_to = -1;
    recompute_state(g);
    fen_refresh(g);
    g->move_follow = true;
}

static void clear_anims(Gui *g)
{
    g->anim_count = 0;
    g->quality_anim=0;
    g->check_anim = 0;
    g->end_anim = 0;
}

/* Schedule position effects after the moving piece arrives. Direct jumps
 * keep the settled end badges, while forward playback celebrates again. */
static bool position_is_draw(const Gui *g)
{
    return g->state == STALEMATE || g->state == INSUFFICIENT_MATERIAL ||
           g->state == THREEFOLD_REPETITION || g->state == FIFTY_MOVE_RULE ||
           (g->mode == MODE_ONLINE && g->online_over &&
            strcmp(g->override_result, "1/2-1/2") == 0);
}

static void start_position_effects(Gui *g)
{
    Uint32 delay = g->anim_count ? g->anim_queue[g->anim_count - 1].dur : 0;
    if (g->state == CHECKMATE || position_is_draw(g)) {
        g->check_anim = 0;
        g->end_anim = g->anim_style == ANIM_NONE ? 0 : SDL_GetTicks() + delay + 1;
    } else if (g->state == NO_GAME_OVER && in_check(&g->board, g->board.side)) {
        g->check_anim = SDL_GetTicks() + 1;
        g->check_anim_sq = find_king(&g->board, g->board.side);
    }
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
    if(g->play_tree)mt_free(g->play_tree);g->play_tree=g->play_live=g->play_view=NULL;
    g->move_hit_count=0;
    g->quality_node=NULL;g->quality_class=RC_NONE;
    g->history_browsing=false;
    clear_selection(g);
    ann_clear(g);
    g->promo_from = g->promo_to = -1;
    clear_anims(g);
    san_close_box(g);
    recompute_state(g);
}

/* Discard a report before replacing its tree and its evaluation snapshots. */
static void clear_review_results(Gui *g)
{
    if (g->review_ai) ai_stop_search(g->review_ai);
    g->review_on = false;
    g->rev_report = false;
    g->review_n = 0;
    memset(g->review_cls, 0, sizeof g->review_cls);
}

/* Replace the position from a FEN string. Returns false on parse failure. */
static bool load_fen(Gui *g, const char *fen)
{
    Board b;
    if (!fen_parse(fen, &b)) return false;
    g->board = b;
    g->ply = 0;
    reset_board_state(g);
    if (g->mode == MODE_ANALYSIS) {
        clear_review_results(g);
        if (g->pgntree) mt_free(g->pgntree);
        g->pgntree = mt_new_root(&g->board);
        g->tree_cur = g->pgntree;
        g->path_len = 0;
    }
    fen_refresh(g);
    analysis_refresh(g);
    return true;
}

/* Describe the visual transition for one move. Board state is already updated
 * by the caller; `pre` is the position before the move. */
static void enqueue_move_anim(Gui *g, const Board *pre, Move m)
{
    if (g->anim_style == ANIM_NONE) { g->anim_fast = false; return; }
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

    s.fast    = g->anim_fast;
    s.cursor_x = g->mouse.x;
    s.cursor_y = g->mouse.y;
    s.cursor_scale = s.fast ? pick_scale(g, SDL_GetTicks(), true) : 1.0f;
    s.dur     = g->anim_fast ? 110 : ANIM_DURS[g->anim_style];
    s.started = false;
    s.start   = 0;
    g->anim_fast = false;
    g->anim_queue[g->anim_count++] = s;
}

/* Enqueue an animation that moves a piece from `from` to `to` (used when
 * stepping back through the game, where the board already shows `from`). */
static void enqueue_anim_manual(Gui *g, Piece piece, int from, int to,
                                Piece captured, int cap_sq, bool reverse)
{
    if (g->anim_style == ANIM_NONE) return;
    if (g->anim_count >= ANIM_QUEUE_MAX) return;
    AnimStep s;
    memset(&s, 0, sizeof s);
    s.piece    = piece;
    s.from     = from;
    s.to       = to;
    s.captured = captured;
    s.cap_sq   = cap_sq;
    s.reverse  = reverse;
    s.dur      = ANIM_DURS[g->anim_style];
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
    if (g->state == STALEMATE || g->state == INSUFFICIENT_MATERIAL ||
        g->state == THREEFOLD_REPETITION || g->state == FIFTY_MOVE_RULE) {
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

/* Live play and local variations keep independent positions. */
static bool play_mode(const Gui *g){return g->mode==MODE_SINGLE||g->mode==MODE_LOCAL||g->mode==MODE_ONLINE;}
static bool play_branch(const Gui *g){return play_mode(g)&&g->history_browsing&&g->play_view;}
static Board *input_board(Gui *g){return play_branch(g)?&g->play_view->board:&g->board;}
static void play_sync(Gui *g)
{
    if(!play_mode(g))return;
    Board *start=g->ply?&g->before[0]:&g->board;
    if(g->play_tree&&!board_rep_equal(start,&g->play_tree->board)){mt_free(g->play_tree);g->play_tree=NULL;g->play_view=NULL;}
    if(!g->play_tree)g->play_tree=mt_new_root(start);
    MoveNode *n=g->play_tree;
    for(int k=0;n&&k<g->ply;k++){
        MoveNode *child=mt_find_child(n,g->history[k]);if(!child)child=mt_add_child(n,g->history[k],g->move_san[k]);
        /* Actual play is always the main line, even after local exploration. */
        if(child&&child!=n->first){if(child->prev)child->prev->next=child->next;if(child->next)child->next->prev=child->prev;else n->last=child->prev;child->prev=NULL;child->next=n->first;n->first->prev=child;n->first=child;}n=child;
    }
    g->play_live=n;
    if(!g->history_browsing)g->play_view=n;
}
static void push_move(Gui *g, Move m)
{
    if (g->ply >= MAX_PLY) return;
    if(g->mode==MODE_LOCAL&&g->p2p_started&&(!g->net||!g->p2p_ready||p2p_finished(g)))return;
    Board pre = g->board;

    /* Did this move match the engine's best for the position before it? */
    bool was_best = false;
    if (g->eval_best_uci[0]) {
        Move bm;
        if (ai_uci_to_move(&pre, g->eval_best_uci, &bm) &&
            MOVE_FROM(bm) == MOVE_FROM(m) && MOVE_TO(bm) == MOVE_TO(m) &&
            (!(MOVE_FLAGS(m) & FLAG_PROMO) || MOVE_PROMO(bm) == MOVE_PROMO(m)))
            was_best = true;
    }
    bool choice_known=g->eval_valid&&g->eval_best_uci[0];
    g->move_was_best[g->ply] = was_best;

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
    start_position_effects(g);
    play_move_sound(g, &pre, m);
    fen_refresh(g);
    if (g->mode == MODE_ANALYSIS && g->pgntree && g->tree_cur &&
        board_rep_equal(&pre, &g->tree_cur->board)) {
        g->tree_cur = mt_add_child(g->tree_cur, m, g->last_san);
        if(g->tree_cur){g->tree_cur->engine_choice_known=choice_known;g->tree_cur->engine_choice=was_best;}
        sync_from_tree(g);
    }
    g->move_follow = true;
    if(play_mode(g)&&g->play_tree)play_sync(g);
    analysis_refresh(g);
}

static void user_push_move(Gui *g,Move m)
{
    if(!play_branch(g)){push_move(g,m);return;}
    if(mt_depth(g->play_view)>=MAX_PLY)return;
    MoveNode *n=mt_find_child(g->play_view,m);if(!n)n=mt_add_child(g->play_view,m,NULL);if(!n)return;
    g->play_view=n;g->history_cursor=mt_depth(n);g->history_browsing=n!=g->play_live;
    clear_selection(g);san_close_box(g);g->promo_from=g->promo_to=-1;g->move_follow=true;
}

static void set_msg(Gui *g, const char *fmt, const char *arg)
{
    if (arg)
        snprintf(g->msg, sizeof g->msg, fmt, arg);
    else
        snprintf(g->msg, sizeof g->msg, "%s", fmt);
    g->msg_until = SDL_GetTicks() + 2500;
}

/* Step one ply forward in the tree, animating the played move. */
static void tree_go_forward(Gui *g)
{
    /* The board already contains the previous move's destination. Dropping
     * its visual transition makes repeated navigation settle immediately. */
    clear_anims(g);
    g->check_anim = 0;
    if (!g->tree_cur || !g->tree_cur->first) return;
    MoveNode *child = g->tree_cur->first;
    enqueue_move_anim(g, &g->tree_cur->board, child->move);
    g->tree_cur = child;
    sync_from_tree(g);
    start_position_effects(g);
    analysis_refresh(g);
}

/* Step one ply back in the tree, animating the move in reverse. */
static void tree_go_back(Gui *g)
{
    clear_anims(g);
    g->check_anim = 0;
    MoveNode *node = g->tree_cur;
    if (!node || !node->parent) return;
    Move m = node->move;
    const Board *pre = &node->parent->board;
    Piece piece = pre->board[MOVE_FROM(m)];
    Piece captured = EMPTY;
    int cap_sq = -1;
    if (MOVE_FLAGS(m) & FLAG_EP) {
        cap_sq = (MOVE_TO(m) & 7) | (MOVE_FROM(m) & ~7);
        captured = pre->board[cap_sq];
    } else if (pre->board[MOVE_TO(m)] != EMPTY) {
        cap_sq = MOVE_TO(m);
        captured = pre->board[cap_sq];
    }
    enqueue_anim_manual(g, piece, MOVE_TO(m), MOVE_FROM(m), captured, cap_sq, true);
    if (g->anim_count && (MOVE_FLAGS(m) & (FLAG_CASTLE_K | FLAG_CASTLE_Q))) {
        AnimStep *s = &g->anim_queue[g->anim_count - 1];
        bool kingside = (MOVE_FLAGS(m) & FLAG_CASTLE_K) != 0;
        s->has_rook = true;
        s->rfrom = MOVE_TO(m) + (kingside ? -1 : 1);
        s->rto = MOVE_TO(m) + (kingside ? 1 : -2);
        s->rook = pre->board[s->rto];
    }
    g->tree_cur = node->parent;
    sync_from_tree(g);
    analysis_refresh(g);
}

static void do_undo(Gui *g)
{
    /* Analysis navigates the move tree (keeps variations). */
    if (g->mode == MODE_ANALYSIS && g->pgntree && tree_synced(g)) {
        tree_go_back(g);
        return;
    }

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
    if (g->mode == MODE_ANALYSIS) {
        clear_review_results(g);
        if (g->pgntree) mt_free(g->pgntree);
        g->pgntree = mt_new_root(&g->board);
        g->tree_cur = g->pgntree;
        g->path_len = 0;
    }
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
    if (g->mode==MODE_PUZZLE&&g->history_browsing)return false;
    if (san_find(input_board(g), buf, &m)) {
        if (g->mode == MODE_PUZZLE) {
            if (!puzzle_human_move(g, m)) {
                puzzle_mistake(g);
                audio_play(SND_ILLEGAL);
                set_msg(g, "Incorrect - try again", NULL);
                return false;
            }
        } else {
            user_push_move(g, m);
        }
        g->input.len = 0;
        g->input.text[0] = 0;
        set_msg(g, "Moved %s", play_branch(g)?g->play_view->san:g->last_san);
        return true;
    }
    audio_play(SND_ILLEGAL);
    set_msg(g, "Illegal move: %s", buf);
    return false;
}

/* Typed SAN is only allowed for the side you control. */
static bool san_can_type(const Gui *g)
{
    if(play_branch(g))return true;
    switch (g->mode) {
        case MODE_SINGLE: return g->board.side == g->human_color;
        case MODE_LOCAL:  return g->board.side == g->local_color&&(!g->p2p_started||(g->net&&g->p2p_ready&&!p2p_finished(g)));
        case MODE_ONLINE: return g->online && !online_spectating(g->online) &&
                                  g->board.side == g->human_color;
        case MODE_ANALYSIS:
        default:          return true;
    }
}

/* Space opens move input; Enter submits it. */
static void san_open_box(Gui *g)
{
    if(g->mode==MODE_PUZZLE)puzzle_prepare_input(g);
    if(g->mode==MODE_PUZZLE&&g->history_browsing)return;
    if (game_state(input_board(g)) != NO_GAME_OVER) return;
    if (!san_can_type(g)) {
        set_msg(g, "Not your turn", NULL);
        return;
    }
    g->san_open = true;
    if (g->mode == MODE_ANALYSIS) g->analysis_summary = false;
    g->input.active = true;
    g->input.len = 0;
    g->input.text[0] = '\0';
}

/* Select a piece (own side) at sq, computing legal target squares. */
static void select_square(Gui *g, int sq)
{
    if(g->mode==MODE_PUZZLE)puzzle_prepare_input(g);
    if (g->mode == MODE_ONLINE && g->online && online_spectating(g->online))
        return;   /* spectators are read-only */
    if(g->mode==MODE_PUZZLE&&g->history_browsing)return;
    if(g->selected==sq){clear_selection(g);return;}
    clear_selection(g);
    Board *position=input_board(g);
    Piece p = position->board[sq];
    if (p == EMPTY) return;
    bool my_piece = (position->side == WHITE) ? WHITE_PIECE(p) : BLACK_PIECE(p);
    if (!my_piece) return;
    if (game_state(input_board(g)) != NO_GAME_OVER) return;

    g->selected = sq;
    g->pick_started = SDL_GetTicks() + 1;
    g->pick_released = 0;
    g->pick_position = g->mouse;
    MoveList legal;
    gen_legal(input_board(g), &legal);
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
        g->anim_fast = false; /* The chooser is a separate click, not a drag. */
        g->promo_from = MOVE_FROM(*m);
        g->promo_to = MOVE_TO(*m);
        g->promo_open_ms = SDL_GetTicks();
        memset(g->promo_hover,0,sizeof g->promo_hover);g->promo_hover_at=g->promo_open_ms;
        clear_selection(g);
        return;
    }
    if (g->mode == MODE_PUZZLE) {
        Move attempt = *m;
        if (puzzle_human_move(g, attempt)) { clear_selection(g); return; }
        puzzle_mistake(g);
        set_msg(g, "Incorrect - try again", NULL);
        audio_play(SND_ILLEGAL);
        clear_selection(g);
        return;
    }
    user_push_move(g, *m);
}

/* Resolve the pending promotion with the chosen piece code. */
static void resolve_promotion(Gui *g, Piece promo)
{
    if (g->promo_from < 0) return;
    int from = g->promo_from;
    int to = g->promo_to;
    g->promo_from = g->promo_to = -1;
    MoveList legal;
    gen_legal(input_board(g), &legal);
    Piece w = promo; /* encoded promotions always use white piece codes */
    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        if (MOVE_FROM(m) == from && MOVE_TO(m) == to &&
            (MOVE_FLAGS(m) & FLAG_PROMO) && MOVE_PROMO(m) == w) {
            if(g->mode==MODE_PUZZLE){
                if(!puzzle_human_move(g,m)){puzzle_mistake(g);audio_play(SND_ILLEGAL);set_msg(g,"Incorrect - try again",NULL);}
            }else user_push_move(g,m);
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
    track->y = g->board_y + 86;
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
    box->x = g->panel_x + 4;
    box->y = g->board_y + 82;
    box->w = 24;
    box->h = 24;
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
    out->x = g->board_x + (g->mode == MODE_ANALYSIS ? 220 : 0);
    out->y = g->board_y + 8*g->sq + (g->mode == MODE_PUZZLE ? (g->ui&&g->ui->reviewing?132:88) : 28);
    out->w = 8*g->sq - (g->mode == MODE_ANALYSIS ? 220 : 0); out->h = 40;
}

static void menu_btn_rect(const Gui *g, SDL_Rect *out)
{
    out->x = g->panel_x + 352; out->y = g->win_h - 120; out->w = 80; out->h = 40;
}

/* FEN load/copy controls sit between the move list and the HUD. */
static void fen_rects(const Gui *g, SDL_Rect *field, SDL_Rect *load, SDL_Rect *copy)
{
    field->x = g->panel_x;       field->y = g->board_y + 480; field->w = 252; field->h = 34;
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
static void open_online(Gui *g, int matchmaking);
static void open_openings(Gui *g);
static void handle_openings_keydown(Gui *g, const SDL_KeyboardEvent *ke);
static void handle_openings_mousedown(Gui *g);
static void handle_openings_textinput(Gui *g, const SDL_TextInputEvent *te);
static void render_openings(Gui *g, SDL_Renderer *ren);
static void open_review_report(Gui *g);
static void handle_review_keydown(Gui *g, const SDL_KeyboardEvent *ke);
static void handle_review_mousedown(Gui *g);
static void render_review_report(Gui *g, SDL_Renderer *ren);

/* Menu entry indices (the runtime list may prepend "Continue"). */
#define MENU_SINGLE     0
#define MENU_PUZZLE     1
#define MENU_ANALYSIS   2
#define MENU_LOCAL      3
#define MENU_ONLINE     4
#define MENU_MATCH      5
#define MENU_APPEARANCE 6
#define MENU_SETTINGS   7
#define MENU_OPENINGS   8
#define MENU_ACCOUNT    9
#define MENU_QUIT       10
#define MENU_COUNT      11

static const char *MENU_ITEMS[MENU_COUNT] = {
    "Singleplayer (vs AI)",
    "Puzzles",
    "Analysis (both sides)",
    "Local Multiplayer",
    "Online Multiplayer",
    "Online Matchmaking",
    "Appearance (boards & pieces)",
    "Settings",
    "Openings",
    "Account",
    "Quit",
};

/* Local Multiplayer needs SDL2_net; the online entries need libwebsockets. */
static bool menu_enabled(const Gui *g, int i)
{
    (void)g;
    if (i == MENU_LOCAL) return net_available();
    if (i == MENU_ONLINE || i == MENU_MATCH)
        return net_p2p_available();
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
    return menu_enabled(g, menu_base_index(g, i));
}

/* Difficulty presets: engine skill (0..20) and think time in ms. */
typedef struct { const char *label; int skill; int movetime; } AiLevel;
static const AiLevel AI_LEVELS[3] = {
    { "Easy",   0,  150 },
    { "Medium", 10, 400 },
    { "Hard",   20, 1000 },
};
#define AI_LEVEL_COUNT 3

#define MENU_TOP          170   /* below the title/subtitle */
#define MENU_AREA_BOTTOM  130   /* reserve for the message line + footer */
#define MENU_ITEM_H       46    /* preferred (max) item height */
#define MENU_GAP          8

/* Adaptive layout: shrink item height/gap so any number of entries fits the
 * area between the subtitle and the footer without overlap. */
static void menu_metrics(const Gui *g, int *ih, int *igap, int *istart)
{
    int n = menu_count(g);
    if (n < 1) n = 1;
    int gap = MENU_GAP;
    int area = (g->win_h - MENU_AREA_BOTTOM) - MENU_TOP;
    if (area < 1) area = 1;

    int h = (area - (n - 1) * gap) / n;
    if (h > MENU_ITEM_H) h = MENU_ITEM_H;
    if (h < 26) { h = 26; gap = 4; }

    int total = n * h + (n - 1) * gap;
    int start = MENU_TOP + (area - total) / 2;
    if (start < MENU_TOP) start = MENU_TOP;
    *ih = h; *igap = gap; *istart = start;
}

static void menu_item_rect(const Gui *g, int i, SDL_Rect *r)
{
    int h, gap, start;
    menu_metrics(g, &h, &gap, &start);

    r->w = 440;
    if (r->w > g->win_w - 160) r->w = g->win_w - 160;
    if (r->w < 240) r->w = 240;
    r->h = h;
    r->x = (g->win_w - r->w) / 2;
    r->y = start + i * (h + gap);
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
    g->review_on = false;
    g->rev_report = false;
    g->analysis_summary = false;
    for (int i = 0; i < MAX_PLY; i++) g->review_cls[i] = RC_NONE;
    if (g->pgntree) mt_free(g->pgntree);
    reset_board_state(g);
    g->pgntree = mt_new_root(&g->board);
    g->tree_cur = g->pgntree;
    g->path_len = 0;
    fen_refresh(g);
    eval_restart(g);
    g->saved.valid = false;
    g->override_result[0] = 0;
}

/* Snapshot the current game so the menu can offer to resume it. Network games
 * cannot be resumed (the peer is gone), so they clear the snapshot instead. */
static void save_game(Gui *g)
{
    if (g->mode == MODE_LOCAL || g->mode == MODE_ONLINE) {
        g->saved.valid = false;
        g->override_result[0] = 0;
        return;
    }

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
    if(g->ui){ui_home(g);return;}
    save_game(g);
    clear_selection(g);
    g->promo_from = g->promo_to = -1;
    g->fen_active = false;
    g->input.active = false;
    g->chat_open = false;
    g->chat_len = 0;
    g->draw_offered = false;
    g->msg[0] = 0;
    if (g->ai_thinking) {
        ai_stop_search(g->ai);
        g->ai_thinking = false;
    }
    if (g->net) {
        if (transport_state(g->net) == NET_STATE_CONNECTED) transport_send(g->net, "BYE");
        transport_close(g->net);
        g->net = NULL;
    }
    if (g->online) { online_destroy(g->online); g->online = NULL; }
    g->net_waiting = false;
    g->net_sent_ply = 0;
    eval_stop(g);
    g->menu_index = g->saved.valid ? 0 : MENU_ANALYSIS;
    g->scene = SCENE_MENU;
}

static void start_single(Gui *g)
{
    eval_stop(g);
    g->scene = SCENE_GAME;
    g->mode = MODE_SINGLE;
    g->human_color = g->setup_side==2?(Color)(rand()%2):(g->setup_side==1?BLACK:WHITE);
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
    g->override_result[0] = 0;
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

    if (g->net && transport_state(g->net) == NET_STATE_CONNECTED) {
        char hello[32];
        snprintf(hello, sizeof hello, "HELLO CHESS1 %s",
                 side == WHITE ? "white" : "black");
        transport_send(g->net, hello);

        char fen[FEN_MAX];
        fen_generate(&g->board, fen, sizeof fen);
        char fbuf[FEN_MAX + 8];
        snprintf(fbuf, sizeof fbuf, "FEN %s", fen);
        transport_send(g->net, fbuf);

        set_msg(g, side == WHITE ? "Connected - you are White"
                                 : "Connected - you are Black", NULL);
    }
    g->saved.valid = false;
    g->override_result[0] = 0;
}

static void promo_rects(Gui *g, SDL_Rect out[4])
{
    SDL_Rect sqr;
    window_sq(g, g->promo_to, &sqr);
    /* Four full board squares, inward from the promotion rank. */
    int size=g->sq, direction=sqr.y<g->board_y+4*g->sq?1:-1;
    for(int i=0;i<4;i++)out[i]=(SDL_Rect){sqr.x,sqr.y+direction*i*size,size,size};
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
    char vmaxfps[16] = "", vsound[8] = "", vpuzzle[16] = "";
    char vonline[128] = "", vnick[32] = "";
    char vaccount[80] = "", vauser[40] = "";
    char voffline[8] = "", vprofile[40] = "", vlocalrating[16] = "";
    char vaccountlocal[8] = "";
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
        if(g->ui&&strcmp(key,"ui_language")==0)g->ui->zh=strcmp(val,"zh")==0;
        else if(strcmp(key,"ui_font")==0)g->font_index=clampi(atoi(val),0,1);
        else if(g->ui&&strcmp(key,"dark_mode")==0)g->ui->dark_mode=atoi(val)!=0;
        else if(g->ui&&strcmp(key,"app_theme")==0)g->ui->app_theme=clampi(atoi(val),0,9);
        else if(g->ui&&strcmp(key,"background_file")==0)snprintf(g->ui->background_file,sizeof g->ui->background_file,"%s",rawval);
        else if(g->ui&&strcmp(key,"background_fit")==0)g->ui->background_fit=atoi(val)!=0;
        else if(g->ui&&strcmp(key,"background_mask")==0)g->ui->background_mask=clampi(atoi(val),0,255);
        else if(g->ui&&strcmp(key,"avatar")==0)g->ui->avatar=atoi(val)%12;
        else if (strcmp(key, "board") == 0) snprintf(vboard, sizeof vboard, "%s", val);
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
        else if (strcmp(key, "puzzle_rating") == 0) snprintf(vpuzzle, sizeof vpuzzle, "%s", val);
        else if (strcmp(key, "online_server") == 0) snprintf(vonline, sizeof vonline, "%s", rawval);
        else if (strcmp(key, "nick") == 0) snprintf(vnick, sizeof vnick, "%s", rawval);
        else if (strcmp(key, "account_token") == 0) snprintf(vaccount, sizeof vaccount, "%s", rawval);
        else if (strcmp(key, "account_user") == 0) snprintf(vauser, sizeof vauser, "%s", rawval);
        else if (strcmp(key, "offline_mode") == 0) snprintf(voffline, sizeof voffline, "%s", val);
        else if (strcmp(key, "local_profile") == 0) snprintf(vprofile, sizeof vprofile, "%s", rawval);
        else if (strcmp(key, "local_rating") == 0) snprintf(vlocalrating, sizeof vlocalrating, "%s", val);
        else if (strcmp(key, "account_local") == 0) snprintf(vaccountlocal, sizeof vaccountlocal, "%s", val);
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
    if (vpuzzle[0])  { int n = atoi(vpuzzle); if (n >= 400) g->puzzle_rating = n; }
    if (vonline[0])  snprintf(g->online_url, sizeof g->online_url, "%s", vonline);
    if (vnick[0])    snprintf(g->online_nick, sizeof g->online_nick, "%s", vnick);
    if (vaccount[0]) snprintf(g->account_token, sizeof g->account_token, "%s", vaccount);
    if (vauser[0])   snprintf(g->account_user, sizeof g->account_user, "%s", vauser);
    if (voffline[0]) g->offline_mode = (atoi(voffline) != 0);
    if (vprofile[0]) snprintf(g->local_profile, sizeof g->local_profile, "%s", vprofile);
    if (vlocalrating[0]) { int n = atoi(vlocalrating); if (n > 0) g->local_rating = n; }
    if (vaccountlocal[0]) g->account_local = (atoi(vaccountlocal) != 0);
    g->offline_mode=true; /* Hosted services are unavailable; P2P stays usable offline. */
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
    fprintf(f,"ui_font = %d\n",g->font_index);
    fprintf(f, "puzzle_rating = %d\n", g->puzzle_rating);
    if(g->ui)fprintf(f,"ui_language = %s\nbackground_file = %s\nbackground_fit = %d\nbackground_mask = %d\navatar = %d\n",g->ui->zh?"zh":"en",g->ui->background_file,g->ui->background_fit,g->ui->background_mask,g->ui->avatar);
    if(g->ui)fprintf(f,"dark_mode = %d\napp_theme = %d\n",g->ui->dark_mode,g->ui->app_theme);
    if (g->engine_path[0]) fprintf(f, "engine = %s\n", g->engine_path);
    if (g->online_url[0]) fprintf(f, "online_server = %s\n", g->online_url);
    if (g->online_nick[0]) fprintf(f, "nick = %s\n", g->online_nick);
    if (g->account_token[0]) fprintf(f, "account_token = %s\n", g->account_token);
    if (g->account_user[0]) fprintf(f, "account_user = %s\n", g->account_user);
    fprintf(f, "offline_mode = %d\n", g->offline_mode ? 1 : 0);
    if (g->local_profile[0]) fprintf(f, "local_profile = %s\n", g->local_profile);
    fprintf(f, "local_rating = %d\n", g->local_rating);
    fprintf(f, "account_local = %d\n", g->account_local ? 1 : 0);
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
    /* Seed once per desktop process, before home previews or puzzle selection.
     * OS-backed SQLite entropy also distinguishes launches within one second. */
    static bool seeded=false;
    if(!seeded){unsigned seed;sqlite3_randomness(sizeof seed,&seed);srand(seed);seeded=true;}
    Gui *g = calloc(1, sizeof *g);
    if (!g) return NULL;
    g->offline_mode=true;
    g->p2p_side=2;g->p2p_time=7;
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
    g->menu_index = MENU_ANALYSIS;  /* Analysis is the default */
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
    g->move_follow = true;
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
    snprintf(g->online_url, sizeof g->online_url, "ws://127.0.0.1:7681/ws");
    snprintf(g->online_nick, sizeof g->online_nick, "Player");
    g->puzzle_band = 2;         /* Normal: around my current rating */
    g->puzzle_theme = 0;        /* any */
    g->puzzle_rating = 1500;
    g->local_rating = 1500;
    snprintf(g->local_profile, sizeof g->local_profile, "Player");
    g->puzzles = puzzles_load(path_puzzles_file());
    {
        char bkpath[1100];
        snprintf(bkpath, sizeof bkpath, "%s/openings.tsv", path_assets());
        g->book = opening_load(bkpath);
    }
    g->review_ai = NULL;
    g->pgntree = NULL;
    const char *ep = ai_find_engine(NULL);
    if (ep) snprintf(g->engine_path, sizeof g->engine_path, "%s", ep);
    board_reset(&g->board);
    recompute_state(g);
    fen_refresh(g);
    ui_init(g);
    return g;
}

static const char *badge_file(int cls)
{
    switch (cls) {
        case RC_FORCED:     return "badge_forced.svg";
        case RC_BOOK:       return "badge_book.svg";
        case RC_BRILLIANT:  return "badge_brilliant.svg";
        case RC_GREAT:      return "badge_great.svg";
        case RC_BEST:       return "badge_best.svg";
        case RC_EXCELLENT:  return "badge_excellent.svg";
        case RC_GOOD:       return "badge_good.svg";
        case RC_INACCURACY: return "badge_inaccuracy.svg";
        case RC_MISTAKE:    return "badge_mistake.svg";
        case RC_BLUNDER:    return "badge_blunder.svg";
        case RC_MISS:       return "badge_miss.svg";
        default:            return NULL;
    }
}

/* Rasterize the vector badges (SVG) into cached textures for crisp scaling. */
static void load_badges(Gui *g)
{
    for (int i = 0; i < 12; i++)
        if (g->badge_tex[i]) { SDL_DestroyTexture(g->badge_tex[i]); g->badge_tex[i] = NULL; }
    if (!g->ren) return;

    const int SZ = 512;
    for (int i = 0; i < 12; i++) {
        const char *fn = badge_file(i);
        if (!fn) continue;
        char path[1200];
        snprintf(path, sizeof path, "%s/badges/%s", path_assets(), fn);

        SDL_Surface *s = NULL;
#if defined(SDL_IMAGE_VERSION_ATLEAST) && SDL_IMAGE_VERSION_ATLEAST(2, 6, 0)
        SDL_RWops *rw = SDL_RWFromFile(path, "rb");
        if (rw) {
            s = IMG_LoadSizedSVG_RW(rw, SZ, SZ);
            SDL_RWclose(rw);
        }
#else
        s = IMG_Load(path);
#endif
        if (!s) { if (getenv("OPENCHESS_DEBUG_BADGES")) fprintf(stderr, "badge load failed: %s\n", path); continue; }
        g->badge_tex[i] = SDL_CreateTextureFromSurface(g->ren, s);
        SDL_FreeSurface(s);
        if (g->badge_tex[i]) {
            SDL_SetTextureBlendMode(g->badge_tex[i], SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
            SDL_SetTextureScaleMode(g->badge_tex[i], SDL_ScaleModeLinear);
#endif
        }
    }
    static const char *end_names[] = { "winner", "checkmate", "draw" };
    for (int i = 0; i < 3; i++) {
        if (g->end_tex[i]) SDL_DestroyTexture(g->end_tex[i]);
        char path[1200];
        snprintf(path, sizeof path, "%s/badges/end_%s.svg", path_assets(),
                 end_names[i]);
        g->end_tex[i] = IMG_LoadTexture(g->ren, path);
        if (g->end_tex[i]) {
            SDL_SetTextureBlendMode(g->end_tex[i], SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
            SDL_SetTextureScaleMode(g->end_tex[i], SDL_ScaleModeLinear);
#endif
        }
    }
}

/* Build a high-resolution anti-aliased ring (with a soft glow) once, so the
 * capture indicator is crisp when scaled down. */
static SDL_Texture *make_target_dot(SDL_Renderer *ren)
{
    const int size=128;
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,size,size,32,SDL_PIXELFORMAT_RGBA32);
    if(!surface)return NULL;
    SDL_LockSurface(surface);
    for(int y=0;y<size;y++)for(int x=0;x<size;x++){
        float dx=x+0.5f-size/2.0f,dy=y+0.5f-size/2.0f,radius=size*0.30f;
        float distance=sqrtf(dx*dx+dy*dy);
        float body=clampi((int)((radius+1.0f-distance)*255.0f),0,255)/255.0f;
        float edge=fmaxf(0,sqrtf(dx*dx+(dy-3.0f)*(dy-3.0f))-radius);
        float shadow=0.19f*expf(-edge*edge/48.0f);
        float face_alpha=body*0.88f,alpha=face_alpha+shadow*(1-face_alpha);
        float lighting=fmaxf(0,fminf(1,(dy+radius)/(2*radius)));
        float face=255.0f-24.0f*lighting,a=shadow*(1-face_alpha);
        Uint8 red=alpha>0?(Uint8)((face*face_alpha+28*a)/alpha):0;
        Uint8 green=alpha>0?(Uint8)(((face+lighting)*face_alpha+43*a)/alpha):0;
        Uint8 blue=alpha>0?(Uint8)(((face+3*lighting)*face_alpha+61*a)/alpha):0;
        ((Uint32 *)((Uint8 *)surface->pixels+y*surface->pitch))[x]=SDL_MapRGBA(surface->format,red,green,blue,(Uint8)(alpha*255));
    }
    SDL_UnlockSurface(surface);
    SDL_Texture *texture=SDL_CreateTextureFromSurface(ren,surface);SDL_FreeSurface(surface);
    if(texture){SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
#endif
    }
    return texture;
}

static SDL_Texture *make_ring_texture(SDL_Renderer *ren, int size,
                                      Uint8 cr, Uint8 cg, Uint8 cb)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32,
                                                    SDL_PIXELFORMAT_RGBA32);
    if (!s) return NULL;
    SDL_LockSurface(s);
    Uint32 *px = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4;
    float c = size / 2.0f;
    float R = size * 0.36f;         /* ring centreline radius */
    float T = size * 0.075f;        /* ring thickness */
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float dx = (float)x + 0.5f - c, dy = (float)y + 0.5f - c;
            float d = sqrtf(dx * dx + dy * dy);
            float edge = fabsf(d - R) - T * 0.5f;   /* <=0 inside the ring */
            float ss = edge / 1.5f;
            if (ss < 0.0f) ss = 0.0f;
            if (ss > 1.0f) ss = 1.0f;
            ss = ss * ss * (3.0f - 2.0f * ss);
            float body = 1.0f - ss;
            float g = (d - R) / (T * 2.4f);
            float glow = expf(-g * g) * 0.5f;
            float a = body > glow ? body : glow;
            if (a > 1.0f) a = 1.0f;
            px[y * pitch + x] = SDL_MapRGBA(s->format, cr, cg, cb,
                                            (Uint8)(a * 255.0f));
        }
    }
    SDL_UnlockSurface(s);
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s);
    if (t) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
#endif
    }
    return t;
}

/* Cache a filled bloom whose inner tail progressively clears. The advancing
 * outer edge is scaled independently; no frame ever contracts the wave. */
static SDL_Texture *make_pick_glow(SDL_Renderer *ren)
{
    const int size = PICK_GLOW_SIZE;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, size * PICK_GLOW_COLS,
                                                   size * (PICK_GLOW_FRAMES / PICK_GLOW_COLS), 32,
                                                   SDL_PIXELFORMAT_RGBA32);
    if (!s) return NULL;
    SDL_LockSurface(s);
    for (int frame = 0; frame < PICK_GLOW_FRAMES; frame++) {
        float tail = 0.90f * frame / (PICK_GLOW_FRAMES - 1);
        for (int y = 0; y < size; y++) {
            int row_y = y + (frame / PICK_GLOW_COLS) * size;
            Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + row_y * s->pitch);
            for (int x = 0; x < size; x++) {
                float dx = (x + 0.5f - size / 2) / (size / 2);
                float dy = (y + 0.5f - size / 2) / (size / 2);
                float radius = sqrtf(dx * dx + dy * dy);
                float edge = (radius - 0.62f) / 0.38f;
                if (edge < 0) edge = 0;
                if (edge > 1) edge = 1;
                float alpha = 1.0f - edge * edge * (3.0f - 2.0f * edge);
                if (frame > 0) {
                    float clear = (radius - tail + 0.16f) / 0.16f;
                    if (clear < 0) clear = 0;
                    if (clear > 1) clear = 1;
                    alpha *= clear * clear * (3.0f - 2.0f * clear);
                }
                row[x + (frame % PICK_GLOW_COLS) * size] =
                    SDL_MapRGBA(s->format, 60, 220, 245, (Uint8)(255 * alpha));
            }
        }
    }
    SDL_UnlockSurface(s);
    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    SDL_FreeSurface(s);
    if (t) {
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
#endif
    }
    return t;
}

void gui_init_assets(Gui *g, SDL_Renderer *ren)
{
    g->ren = ren;
    ui_assets(g);
    g->win = SDL_RenderGetWindow(ren);

    /* Size the window to the magnified base canvas and allow zooming out to 0.5. */
    if (g->win) {
        SDL_SetWindowMinimumSize(g->win,1024,720);
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
    load_badges(g);
    if (!g->capture_ring)
        g->capture_ring = make_ring_texture(ren, 256, 225, 80, 70);
    if(!g->target_dot)g->target_dot=make_target_dot(ren);
    if (!g->pick_glow) g->pick_glow = make_pick_glow(ren);
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
    ui_close(g);
    engine_install_free(g->engine_install);
    if (g->ai) ai_stop(g->ai);
    if (g->eval_ai) ai_stop(g->eval_ai);
    if (g->net) {
        if (transport_state(g->net) == NET_STATE_CONNECTED) transport_send(g->net, "BYE");
        transport_close(g->net);
    }
    if (g->online) online_destroy(g->online);
    puzzles_free(g->puzzles);
    opening_free(g->book);
    if (g->review_ai) ai_stop(g->review_ai);
    if (g->pgntree) mt_free(g->pgntree);
    if(g->play_tree)mt_free(g->play_tree);
    destroy_piece_textures(g);
    for (int i = 0; i < 12; i++)
        if (g->badge_tex[i]) SDL_DestroyTexture(g->badge_tex[i]);
    if (g->capture_ring) SDL_DestroyTexture(g->capture_ring);
    if (g->target_dot) SDL_DestroyTexture(g->target_dot);
    if (g->pick_glow) SDL_DestroyTexture(g->pick_glow);
    for (int i = 0; i < 3; i++)
        if (g->end_tex[i]) SDL_DestroyTexture(g->end_tex[i]);
    if (g->tex_board) SDL_DestroyTexture(g->tex_board);
    destroy_thumbs(&g->board_thumbs, g->boards.count);
    destroy_thumbs(&g->piece_thumbs, g->pieces.count);
    themes_free(&g->boards);
    themes_free(&g->pieces);
    if (g->font_piece[0]) TTF_CloseFont(g->font_piece[0]);
    if (g->font_piece[1]) TTF_CloseFont(g->font_piece[1]);
    for(int i=0;i<2;i++){if(g->font_preview[i])TTF_CloseFont(g->font_preview[i]);g->font_preview[i]=NULL;}
    if (g->font_ui) TTF_CloseFont(g->font_ui);
    if (g->font_small) TTF_CloseFont(g->font_small);
    if (g->font_tiny) TTF_CloseFont(g->font_tiny);
    if (g->font_rating) TTF_CloseFont(g->font_rating);
    if (g->font_move) TTF_CloseFont(g->font_move);
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
        case MENU_SINGLE:
            g->setup_field = 0;
            g->menu_msg[0] = 0;
            g->scene = SCENE_SINGLE_SETUP;
            break;
        case MENU_PUZZLE:
            if (!g->puzzles) {
                snprintf(g->menu_msg, sizeof g->menu_msg,
                         "No puzzles found - run scripts/import_puzzles.py");
                break;
            }
            g->menu_msg[0] = 0;
            g->scene = SCENE_PUZZLE_SETUP;
            break;
        case MENU_ANALYSIS: start_analysis(g); break;
        case MENU_LOCAL:
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
        case MENU_ONLINE:
        case MENU_MATCH:
            p2p_open(g);
            break;
        case MENU_APPEARANCE: open_appearance(g, SCENE_MENU); break;
        case MENU_SETTINGS: open_settings_scene(g, SCENE_MENU); break;
        case MENU_OPENINGS:
            if (!g->book || opening_line_count(g->book) == 0) {
                snprintf(g->menu_msg, sizeof g->menu_msg,
                         "No opening book - run scripts/import_openings.sh");
                break;
            }
            open_openings(g);
            break;
        case MENU_ACCOUNT:
            /* Offline (or without libwebsockets): a standalone local account. */
            if (g->offline_mode || !net_ws_available()) {
                g->account_register = false;
                g->account_open = true;
                g->account_focus = 0;
                g->account_user_in[0] = g->account_pass_in[0] = '\0';
                break;
            }
            open_online(g, 0);
            open_account(g);
            break;
        case MENU_QUIT: g->quit = true; break;
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

/* Online/offline pill in the top-right of the welcome screen. */
static void menu_mode_rect(const Gui *g, SDL_Rect *r)
{
    r->w = 132; r->h = 34;
    r->x = g->win_w - r->w - 24;
    r->y = 24;
}

static void toggle_offline_mode(Gui *g)
{
    g->offline_mode=true;
    if(g->online){online_destroy(g->online);g->online=NULL;}
    set_msg(g,ui_tr(g,"Use P2P to play with a friend; no online account needed.","请通过 P2P 与好友对弈，无需在线账号。"),NULL);
}

static void handle_menu_mousedown(Gui *g)
{
    SDL_Rect pill;
    menu_mode_rect(g, &pill);
    if (pt_in(&pill, g->mouse.x, g->mouse.y)) { toggle_offline_mode(g); return; }

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

static void setup_rects(const Gui *g, SDL_Rect sides[3], SDL_Rect levels[3],
                        SDL_Rect *start, SDL_Rect *back)
{
    int bw = 120, bh = 48, gap = 20;
    int x0 = 208+(g->win_w-208-(3*bw+2*gap))/2;
    for(int i=0;i<3;i++)sides[i]=(SDL_Rect){x0+i*(bw+gap),300,bw,bh};

    int lg = 16;
    int lx = 208+(g->win_w-208-(3*bw+2*lg))/2;
    for (int i = 0; i < 3; i++)
        levels[i] = (SDL_Rect){ lx + i * (bw + lg), 420, bw, bh };

    start->w = 150; start->h = 46; start->x = (208+(g->win_w-208)/2) - 160; start->y = 540;
    back->w  = 150; back->h  = 46; back->x  = (208+(g->win_w-208)/2) + 10;  back->y  = 540;
}

static void setup_change(Gui *g, int dir)
{
    if (g->setup_field == 0)
        g->setup_side = (g->setup_side + dir + 3) % 3;
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
    SDL_Rect sides[3], levels[3], start, back;
    setup_rects(g, sides, levels, &start, &back);
    SDL_Point p = g->mouse;

    for (int i = 0; i < 3; i++)
        if (pt_in(&sides[i], p.x, p.y)) { g->setup_field = 0; g->setup_side = i; return; }
    for (int i = 0; i < 3; i++)
        if (pt_in(&levels[i], p.x, p.y)) { g->setup_field = 1; g->setup_level = i; return; }
    if (pt_in(&start, p.x, p.y)) { setup_begin(g); return; }
    if (pt_in(&back, p.x, p.y)) { g->scene = SCENE_MENU; return; }
}

/* ---- puzzles ---- */

typedef struct { const char *label; int offset; } PuzzleBand;
static const PuzzleBand PUZZLE_BANDS[] = {
    {"Very Easy",-600},{"Easy",-300},{"Normal",0},{"Hard",300},{"Very Hard",600}
};
#define PUZZLE_BAND_COUNT ((int)(sizeof PUZZLE_BANDS / sizeof PUZZLE_BANDS[0]))

static const char *const PUZZLE_THEMES[] = {
    "Any", "mateIn1", "mateIn2", "mateIn3", "fork", "pin", "skewer",
    "discoveredAttack", "deflection", "attraction", "sacrifice",
    "defensiveMove", "kingsideAttack", "queensideAttack", "endgame",
};
#define PUZZLE_THEME_COUNT ((int)(sizeof PUZZLE_THEMES / sizeof PUZZLE_THEMES[0]))

static const char *const PUZZLE_THEME_LABELS[]={
    "All themes","Mate in one","Mate in two","Mate in three","Fork","Pin","Skewer",
    "Discovered attack","Deflection","Attraction","Sacrifice","Defensive move",
    "Kingside attack","Queenside attack","Endgame"
};
static const char *puzzle_theme_label(Gui *g,int index)
{
    return g->ui&&g->ui->zh?ui_old_text(g,PUZZLE_THEMES[index]):PUZZLE_THEME_LABELS[index];
}

static void puzzle_split_moves(Gui *g)
{
    g->puzzle_nmoves = 0;
    const char *p = g->puzzle_cur.moves;
    while (*p && g->puzzle_nmoves < 32) {
        while (*p == ' ') p++;
        if (!*p) break;
        int i = 0;
        while (*p && *p != ' ' && i < 5) g->puzzle_moves[g->puzzle_nmoves][i++] = *p++;
        g->puzzle_moves[g->puzzle_nmoves][i] = 0;
        if (i >= 4) g->puzzle_nmoves++;
    }
}

/* Elo update: expected score from the user vs. puzzle rating; K scales down a
 * little as the rating rises so it stays responsive at low ratings. */
static void puzzle_score(Gui *g, bool solved)
{
    if(g->ui){ui_puzzle_settle(g,solved);return;}
    double ur = g->puzzle_rating;
    double pr = g->puzzle_cur.rating;
    double exp = 1.0 / (1.0 + pow(10.0, (pr - ur) / 400.0));
    double K = ur < 2000.0 ? 40.0 : 24.0;
    double nu = ur + K * ((solved ? 1.0 : 0.0) - exp);
    if (nu < 400) nu = 400;
    if (nu > 3000) nu = 3000;
    g->puzzle_rating = (int)(nu + 0.5);
    g->puzzle_delta = g->puzzle_rating - (int)(ur + 0.5);
    g->config_dirty = true;
    if (g->online && online_logged_in(g->online))
        online_puzzle_result(g->online, g->puzzle_cur.rating, solved);
}

/* A wrong attempt: score the puzzle as incorrect once (rating drops), but keep
 * letting the player retry so they can learn from the mistake. */
static void puzzle_mistake(Gui *g)
{
    if(g->ui&&(g->ui->reviewing||(g->ui->current_entry>=0&&g->ui->entries[g->ui->current_entry].day)))return;
    if (g->puzzle_counted) return;
    g->puzzle_counted = true;
    puzzle_score(g, false);
}

static void open_puzzle(Gui *g)
{
    if(g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE&&!g->puzzle_done&&!g->puzzle_failed&&!g->puzzle_solution_visible){
        set_msg(g,"Finish or reveal this puzzle before continuing.",NULL);return;
    }
    if(g->ui){int day=0;if(g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE&&g->ui->current_entry>=0)day=g->ui->entries[g->ui->current_entry].day;
        if(day){int next=day<workspace_today()?day+1:day;g->ui->selected_day=g->ui->month_day=next;ui_next_puzzle(g,next);}else ui_next_puzzle(g,0);return;}
    if (!g->puzzles) return;
    const PuzzleBand *b = &PUZZLE_BANDS[g->puzzle_band];
    int rmin=g->puzzle_rating+b->offset-150,rmax=g->puzzle_rating+b->offset+149;
    if(rmin<400)rmin=400;if(rmax<400)rmax=400;if(rmax>4000)rmax=4000;
    const char *theme = g->puzzle_theme > 0 ? PUZZLE_THEMES[g->puzzle_theme] : NULL;
    if (!puzzles_pick(g->puzzles, rmin, rmax, theme, &g->puzzle_cur)) {
        snprintf(g->menu_msg, sizeof g->menu_msg, "No puzzle matches that filter");
        return;
    }
    puzzle_split_moves(g);
    if (g->puzzle_nmoves < 2) { set_msg(g, "Bad puzzle data", NULL); return; }

    Board b2;
    if (!fen_parse(g->puzzle_cur.fen, &b2)) {
        set_msg(g, "Bad puzzle FEN", NULL);
        return;
    }

    g->mode = MODE_PUZZLE;
    g->scene = SCENE_GAME;
    g->flipped = false;
    g->auto_flip = false;
    g->puzzle_done = false;
    g->puzzle_failed = false;
    g->puzzle_wrong=false;g->puzzle_badge_kind=0;g->puzzle_solution_visible=false;g->puzzle_had_error=false;
    g->puzzle_counted = false;
    g->puzzle_delta = 0;
    g->input.len = 0; g->input.text[0] = 0; g->input.active = false;
    g->fen_active = false;
    g->saved.valid = false;
    g->override_result[0] = 0;

    g->board = b2;
    g->ply = 0;
    reset_board_state(g);
    fen_refresh(g);

    Move m;
    if (uci_to_move(&g->board, g->puzzle_moves[0], &m)) push_move(g, m);
    g->puzzle_step = 1;
    g->human_color = g->board.side;
    g->flipped = (g->human_color == BLACK);
    snprintf(g->white_name, sizeof g->white_name, "Puzzle");
    snprintf(g->black_name, sizeof g->black_name, "You");
}

/* Rebuild only the played prefix; settlement and hint/reveal flags stay intact. */
static void puzzle_rewind(Gui *g,int step)
{
    if(g->mode!=MODE_PUZZLE||g->puzzle_nmoves<1)return;
    if(step<1)step=1;if(step>g->puzzle_nmoves)step=g->puzzle_nmoves;
    if(step%2==0&&step>1)step--;
    Board root;if(!fen_parse(g->puzzle_cur.fen,&root))return;
    g->board=root;g->ply=0;g->puzzle_step=0;reset_board_state(g);
    memset(g->review_cls,0,sizeof g->review_cls);memset(g->move_san,0,sizeof g->move_san);
    for(int k=0;k<step;k++){Move move;if(!uci_to_move(&g->board,g->puzzle_moves[k],&move))break;push_move(g,move);g->puzzle_step++;}
    clear_anims(g);g->puzzle_wrong=false;g->puzzle_badge_kind=0;g->puzzle_done=false;g->puzzle_failed=false;
    g->move_scroll=0;g->move_hit_count=0;fen_refresh(g);
    if(g->ui&&g->ui->current_entry>=0)g->ui->entries[g->ui->current_entry].step=g->puzzle_step;
}

static void history_seek(Gui *g,int ply)
{
    if(play_mode(g)){play_sync(g);MoveNode *n=g->play_tree;for(int i=0;n&&n->first&&i<ply;i++)n=n->first;g->play_view=n;}
    int maximum=g->mode==MODE_PUZZLE&&g->puzzle_solution_visible?g->puzzle_nmoves:g->ply;
    g->history_cursor=clampi(ply,0,maximum);g->history_browsing=g->history_cursor!=g->ply;
    clear_selection(g);san_close_box(g);g->promo_from=g->promo_to=-1;g->dragging=false;g->move_follow=true;
    if(g->mode==MODE_PUZZLE&&g->puzzle_wrong)g->puzzle_feedback_at=SDL_GetTicks();
}
static bool history_board(Gui *g,Board *out)
{
    if(!g->history_browsing)return false;
    int ply=g->history_cursor;
    if(play_branch(g)){*out=g->play_view->board;return true;}
    if(g->mode==MODE_PUZZLE&&g->puzzle_solution_visible){
        if(!fen_parse(g->puzzle_cur.fen,out))return false;
        for(int k=0;k<ply;k++){Move move;if(!uci_to_move(out,g->puzzle_moves[k],&move))return false;make_move_plumb(out,move);}
    }else if(ply<g->ply)*out=g->before[ply];else *out=g->board;
    return true;
}
static void history_navigate(Gui *g,int delta)
{
    if(g->mode==MODE_ANALYSIS&&g->pgntree&&g->tree_cur){
        for(int i=0;i<abs(delta);i++){if(delta<0)tree_go_back(g);else tree_go_forward(g);}return;
    }
    if(play_mode(g)){play_sync(g);MoveNode *n=g->history_browsing?g->play_view:g->play_live;
        for(int i=0;n&&i<abs(delta);i++){MoveNode *next=delta<0?n->parent:n->first;if(!next)break;n=next;}
        if(n){g->play_view=n;g->history_cursor=mt_depth(n);g->history_browsing=n!=g->play_live;clear_selection(g);san_close_box(g);g->promo_from=g->promo_to=-1;g->move_follow=true;}return;
    }
    history_seek(g,(g->history_browsing?g->history_cursor:g->ply)+delta);
}
static void puzzle_prepare_input(Gui *g)
{
    if(g->mode!=MODE_PUZZLE)return;
    if(g->history_browsing)return;
    if(g->puzzle_wrong)puzzle_rewind(g,g->puzzle_step);
}

static bool puzzle_human_move(Gui *g, Move m)
{
    if (g->mode != MODE_PUZZLE) return false;
    if (g->puzzle_done || g->puzzle_failed) return false;
    if (g->puzzle_step >= g->puzzle_nmoves) return false;

    Move want;
    if (!uci_to_move(&g->board, g->puzzle_moves[g->puzzle_step], &want)) return false;
    if (MOVE_FROM(m) != MOVE_FROM(want) || MOVE_TO(m) != MOVE_TO(want) || MOVE_PROMO(m)!=MOVE_PROMO(want)){
        push_move(g,m);g->puzzle_wrong=true;g->puzzle_had_error=true;
        if(g->ui&&g->ui->current_entry>=0)g->ui->entries[g->ui->current_entry].had_error=true;
        g->puzzle_badge_sq=MOVE_TO(m);g->puzzle_badge_kind=-1;g->puzzle_feedback_at=SDL_GetTicks();
        return false;
    }

    push_move(g, want);
    g->puzzle_wrong=false;g->puzzle_badge_sq=MOVE_TO(want);g->puzzle_badge_kind=1;g->puzzle_feedback_at=SDL_GetTicks();
    g->puzzle_step++;
    if (g->puzzle_step < g->puzzle_nmoves) {
        Move r;
        if (uci_to_move(&g->board, g->puzzle_moves[g->puzzle_step], &r)) {
            push_move(g, r);
            g->puzzle_step++;
        }
    }
    if (g->puzzle_step >= g->puzzle_nmoves) {
        g->puzzle_done = true;
        if (!g->puzzle_counted) puzzle_score(g, true);
        set_msg(g, "Solved!", NULL);
    }
    return true;
}

static void puzzle_hint(Gui *g)
{
    if(g->mode==MODE_PUZZLE)puzzle_prepare_input(g);
    if(g->ui&&!g->ui->reviewing&&g->ui->current_entry>=0)g->ui->entries[g->ui->current_entry].hint=true;
    if (g->mode != MODE_PUZZLE || g->puzzle_done || g->puzzle_failed) return;
    if (g->puzzle_step < g->puzzle_nmoves)
        set_msg(g, "Hint: %s", g->puzzle_moves[g->puzzle_step]);
}

static void puzzle_reveal(Gui *g)
{
    if(g->mode==MODE_PUZZLE){history_seek(g,g->ply);puzzle_prepare_input(g);}
    if(g->ui&&!g->ui->reviewing&&g->ui->current_entry>=0)g->ui->entries[g->ui->current_entry].reveal=true;
    if (g->mode != MODE_PUZZLE || g->puzzle_done || g->puzzle_failed) return;
    g->puzzle_failed = true;g->puzzle_solution_visible=true;
    if (!g->puzzle_counted) {g->puzzle_counted = true;puzzle_score(g, false);}
    /* Show the full legal solution as SAN rows, with no answer text overlay. */
    while(g->puzzle_step<g->puzzle_nmoves){
        Move move;if(!uci_to_move(&g->board,g->puzzle_moves[g->puzzle_step],&move))break;
        push_move(g,move);g->puzzle_step++;
    }
    clear_anims(g);g->puzzle_badge_kind=0;g->move_scroll=0;g->move_hit_count=0;g->msg[0]=0;
    if(g->ui&&g->ui->current_entry>=0)g->ui->entries[g->ui->current_entry].step=g->puzzle_step;
}

static void puzzle_setup_rects(const Gui *g, SDL_Rect bands[PUZZLE_BAND_COUNT],
                               SDL_Rect *theme, SDL_Rect *start, SDL_Rect *back)
{
    int bw = (g->win_w-248-4*12)/5, bh = 46, gap = 12;
    if(bw>150)bw=150;
    int total = PUZZLE_BAND_COUNT * bw + (PUZZLE_BAND_COUNT - 1) * gap;
    int x0 = 208+(g->win_w-208-total)/2;
    for (int i = 0; i < PUZZLE_BAND_COUNT; i++)
        bands[i] = (SDL_Rect){ x0 + i * (bw + gap), 300, bw, bh };
    theme->w = 320; theme->h = 44; theme->x = (208+(g->win_w-208)/2) - 160; theme->y = 400;
    start->w = 150; start->h = 46; start->x = (208+(g->win_w-208)/2) - 160; start->y = 520;
    back->w  = 150; back->h  = 46; back->x  = (208+(g->win_w-208)/2) + 10;  back->y  = 520;
}

static void handle_puzzle_setup_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;
    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if(g->ui&&g->ui->theme_open){
        if(k==SDLK_ESCAPE){g->ui->theme_open=false;return;}
        if(k==SDLK_UP||k==SDLK_DOWN){g->puzzle_theme=(g->puzzle_theme+(k==SDLK_DOWN?1:PUZZLE_THEME_COUNT-1))%PUZZLE_THEME_COUNT;return;}
        if(k==SDLK_RETURN||k==SDLK_SPACE){g->ui->theme_open=false;return;}
        return;
    }
    if (k == SDLK_ESCAPE) { g->scene = SCENE_MENU; return; }
    if (k == SDLK_UP || k == SDLK_DOWN || k == SDLK_TAB) { g->setup_field ^= 1; return; }
    if (k == SDLK_LEFT || k == SDLK_RIGHT) {
        int d = (k == SDLK_RIGHT) ? 1 : -1;
        g->puzzle_theme = (g->puzzle_theme + d + PUZZLE_THEME_COUNT) % PUZZLE_THEME_COUNT;
        return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {if(g->setup_field==1&&g->ui)g->ui->theme_open=true;else open_puzzle(g);}
}

static void handle_puzzle_setup_mousedown(Gui *g)
{
    SDL_Rect bands[PUZZLE_BAND_COUNT], theme, start, back;
    puzzle_setup_rects(g, bands, &theme, &start, &back);
    SDL_Point p = g->mouse;
    if(g->ui&&g->ui->theme_open){
        SDL_Rect list={theme.x,theme.y+theme.h+4,theme.w,PUZZLE_THEME_COUNT*26};
        if(pt_in(&list,p.x,p.y))g->puzzle_theme=(p.y-list.y)/26;
        g->ui->theme_open=false;return;
    }

    if (pt_in(&theme, p.x, p.y)) {
        g->setup_field = 1;
        if(g->ui)g->ui->theme_open=true;
        return;
    }
    if (pt_in(&start, p.x, p.y)) { open_puzzle(g); return; }
    if (pt_in(&back, p.x, p.y)) { g->scene = SCENE_MENU; return; }
}

static void render_puzzle_setup(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);
    render_text_centered(g, g->font_ui, "Puzzles", (208+(g->win_w-208)/2), 150,
                         (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect bands[PUZZLE_BAND_COUNT], theme, start, back;
    puzzle_setup_rects(g, bands, &theme, &start, &back);

    bool smart=g->ui&&g->ui->smart;
    render_text_centered(g,g->font_small,smart?ui_tr(g,"Smart practice · Adaptive difficulty","智能练习 · 自动调整难度"):ui_tr(g,"Adjust difficulty while solving","可在做题界面随时调整难度"),208+(g->win_w-208)/2,300,(SDL_Color){150,180,200,255});

    render_text_centered(g, g->font_small, "Theme", (208+(g->win_w-208)/2), 372,
                         (SDL_Color){ 150, 180, 200, 255 });
    char tl[96];snprintf(tl,sizeof tl,"%s   v",puzzle_theme_label(g,g->puzzle_theme));
    ui_button(g,theme,tl,-1,false);

    ui_button(g,start,ui_old_text(g,"Start"),-1,true);
    ui_button(g,back,ui_old_text(g,"Back"),-1,false);

    char info[128];
    snprintf(info, sizeof info, ui_tr(g,"%d puzzles loaded    Your puzzle rating: %d","已加载 %d 道题目    当前解题评分：%d"),
             puzzles_count(g->puzzles), g->puzzle_rating);
    render_text_centered(g, g->font_small, info, (208+(g->win_w-208)/2), 620,
                         (SDL_Color){ 150, 210, 230, 255 });
    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, (208+(g->win_w-208)/2), 650,
                             (SDL_Color){ 255, 170, 120, 255 });
    if(g->ui&&g->ui->theme_open){
        SDL_Rect list={theme.x,theme.y+theme.h+4,theme.w,PUZZLE_THEME_COUNT*26};
        ui_round(ren,list,ui_surface_color((SDL_Color){250,253,255,255}),8);
        for(int i=0;i<PUZZLE_THEME_COUNT;i++){
            SDL_Rect row={list.x+3,list.y+i*26,list.w-6,26};
            if(i==g->puzzle_theme||pt_in(&row,g->mouse.x,g->mouse.y))ui_move_background(g,row,true);
            render_text(g,g->font_small,puzzle_theme_label(g,i),row.x+12,row.y+4,(SDL_Color){235,235,235,255});
        }
    }

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
    g->p2p_started=false;
    if (g->net) { transport_close(g->net); g->net = NULL; }
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
    g->p2p_started=false;
    if (g->net) { transport_close(g->net); g->net = NULL; }
    g->net = net_join(g->net_addr, hj_port(g));
    if (!g->net) {
        snprintf(g->menu_msg, sizeof g->menu_msg,
                 "Could not connect to %s:%s", g->net_addr, g->net_port);
        return;
    }
    enter_local_game(g, transport_role(g->net) == NET_ROLE_JOIN ? BLACK : WHITE);
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

/* ---- online lobby + game ---- */

static void online_close(Gui *g)
{
    if (g->online) { online_destroy(g->online); g->online = NULL; }
}

static void open_online(Gui *g, int matchmaking)
{
    (void)matchmaking;p2p_open(g);
}

/* Connect if we have no live session; rebuild one if it closed. */
static bool online_ensure(Gui *g)
{
    /* Public hosted services are retired. Keep wire compatibility for old saves. */
    (void)g;return false;
}

typedef struct { const char *label; int time_ms; int inc_ms; } OnlineTime;
static const OnlineTime ONLINE_TIMES[] = {
    { "Unlimited",        0, 0 },
    { "5+0",         5*60000, 0 },
    { "10+0",       10*60000, 0 },
    { "10+5",       10*60000, 5000 },
    { "15+10",      15*60000, 10000 },
};
#define ONLINE_TIME_COUNT ((int)(sizeof ONLINE_TIMES / sizeof ONLINE_TIMES[0]))

/* Focus layout: 0 = server, 1 = time, then code+buttons (room) or buttons. */
static int online_focus_max(const Gui *g)
{
    return g->online_ui == 1 ? 4 : 6;   /* room adds a Spectate button */
}

static void online_cycle_time(Gui *g, int dir)
{
    g->online_time_idx = (g->online_time_idx + dir + ONLINE_TIME_COUNT) % ONLINE_TIME_COUNT;
}

static void online_time_control(const Gui *g, int *time_ms, int *inc_ms)
{
    int i = g->online_time_idx;
    if (i < 0 || i >= ONLINE_TIME_COUNT) i = 0;
    if (time_ms) *time_ms = ONLINE_TIMES[i].time_ms;
    if (inc_ms)  *inc_ms  = ONLINE_TIMES[i].inc_ms;
}

static void online_primary(Gui *g)
{
    if (!online_ensure(g)) return;
    int t, inc;
    online_time_control(g, &t, &inc);
    if (g->online_ui == 1) online_queue(g->online, t, inc);
    else                   online_create_room(g->online, t, inc);
}

static void online_secondary(Gui *g)
{
    if (!online_ensure(g)) return;
    if (g->online_ui == 1) {
        online_cancel_queue(g->online);
        return;
    }
    if (!g->online_code_in[0]) {
        snprintf(g->menu_msg, sizeof g->menu_msg, "Enter a room code to join");
        return;
    }
    online_join_room(g->online, g->online_code_in);
}

static void online_spectate_action(Gui *g)
{
    if (!online_ensure(g)) return;
    if (!g->online_code_in[0]) {
        snprintf(g->menu_msg, sizeof g->menu_msg, "Enter a room code to spectate");
        return;
    }
    online_spectate(g->online, g->online_code_in);
}

static void online_back(Gui *g)
{
    online_close(g);
    g->scene = SCENE_MENU;
}

/* Switch from the lobby into the game when the server says it started. */
static void enter_online_game(Gui *g)
{
    g->scene = SCENE_GAME;
    g->mode = MODE_ONLINE;
    g->human_color = online_color(g->online);
    g->auto_flip = true;
    g->flipped = (g->human_color == BLACK);
    const char *opp = online_opponent(g->online);
    if (g->human_color == WHITE) {
        snprintf(g->white_name, sizeof g->white_name, "You");
        snprintf(g->black_name, sizeof g->black_name, "%s", opp);
    } else {
        snprintf(g->white_name, sizeof g->white_name, "%s", opp);
        snprintf(g->black_name, sizeof g->black_name, "You");
    }
    g->menu_msg[0] = 0;
    g->msg[0] = 0;
    g->input.len = 0;
    g->input.text[0] = 0;
    g->input.active = false;
    g->fen_active = false;
    g->chat_open = false;
    g->chat_len = 0;
    if (online_spectating(g->online) && online_fen(g->online)[0]) {
        load_fen(g, online_fen(g->online));
        g->ply = online_ply(g->online);
    } else {
        board_reset(&g->board);
        g->ply = 0;
        reset_board_state(g);
        fen_refresh(g);
    }
    g->net_sent_ply = g->ply;
    g->online_over = false;
    g->draw_offered = false;
    g->saved.valid = false;
    g->override_result[0] = 0;
}

static void online_lobby_tick(Gui *g)
{
    if (!g->online) return;
    online_poll(g->online);
    char uci[8];
    int ply;
    OnlineEvent ev;
    while ((ev = online_next_event(g->online, uci, sizeof uci, &ply)) != ONLINE_EV_NONE) {
        if (ev == ONLINE_EV_START || ev == ONLINE_EV_SPECTATE) {
            enter_online_game(g);
            return;
        }
        if (ev == ONLINE_EV_AUTH) {
            if (online_logged_in(g->online)) {
                snprintf(g->account_token, sizeof g->account_token, "%s",
                         online_auth_token(g->online));
                snprintf(g->account_user, sizeof g->account_user, "%s",
                         online_username(g->online));
                g->puzzle_rating = online_puzzle_rating(g->online);
                g->config_dirty = true;
                set_msg(g, "Logged in as %s", online_username(g->online));
                g->account_open = false;
            } else if (g->account_open) {
                snprintf(g->menu_msg, sizeof g->menu_msg, "Login failed: %s",
                         online_last_error(g->online));
            }
        }
    }
}

/* In-game online pump: relay moves both ways and report peer/game events. */
static void online_tick(Gui *g)
{
    if (!g->online) { go_to_menu(g); return; }
    online_poll(g->online);

    char uci[8];
    int ply;
    OnlineEvent ev;
    while ((ev = online_next_event(g->online, uci, sizeof uci, &ply)) != ONLINE_EV_NONE) {
        if (ev == ONLINE_EV_MOVE) {
            /* The server echoes our own moves too; only apply the opponent's
             * (the next ply we have not played) and never re-apply our own. */
            if (ply == g->ply && g->state == NO_GAME_OVER) {
                Move m;
                if (ai_uci_to_move(&g->board, uci, &m)) push_move(g, m);
                g->net_sent_ply = g->ply;
            }
        } else if (ev == ONLINE_EV_STATE) {
            const char *fen = online_fen(g->online);
            if (fen && *fen && load_fen(g, fen)) {
                g->ply = online_ply(g->online);   /* stay in step with the server */
                g->net_sent_ply = g->ply;
                set_msg(g, "Position resynced", NULL);
            }
        } else if (ev == ONLINE_EV_START) {
            /* Rematch (or the initial start): reset and swap as instructed. */
            enter_online_game(g);
        } else if (ev == ONLINE_EV_REJECT) {
            set_msg(g, "Move rejected: %s", online_last_error(g->online));
        } else if (ev == ONLINE_EV_GAMEOVER) {
            const char *r = online_result(g->online);
            g->online_over = true;
            if (r && *r) snprintf(g->override_result, sizeof g->override_result, "%s", r);
            if (position_is_draw(g)) start_position_effects(g);
            set_msg(g, "Game over: %s", r && *r ? r : "result");
        } else if (ev == ONLINE_EV_OPPONENT_LEFT) {
            set_msg(g, "Opponent disconnected - waiting for them to return", NULL);
        } else if (ev == ONLINE_EV_OPPONENT_JOINED) {
            set_msg(g, "Opponent reconnected", NULL);
        } else if (ev == ONLINE_EV_REMATCH) {
            set_msg(g, "Opponent wants a rematch - press Rematch", NULL);
        } else if (ev == ONLINE_EV_DRAW_OFFER) {
            g->draw_offered = true;
            set_msg(g, "Draw offered", NULL);
        } else if (ev == ONLINE_EV_DRAW_DECLINE) {
            g->draw_offered = false;
            set_msg(g, "Draw offer declined", NULL);
        } else if (ev == ONLINE_EV_SPECTATE) {
            enter_online_game(g);
        } else if (ev == ONLINE_EV_AUTH) {
            if (online_logged_in(g->online)) {
                g->puzzle_rating = online_puzzle_rating(g->online);
                snprintf(g->account_token, sizeof g->account_token, "%s",
                         online_auth_token(g->online));
                g->config_dirty = true;
                char rb[48];
                snprintf(rb, sizeof rb, "Rating updated: PvP %d",
                         online_pvp_rating(g->online));
                set_msg(g, rb, NULL);
            }
        }
    }

    /* Move any queued chat into the on-screen log. */
    {
        char from[40], text[160];
        while (online_take_chat(g->online, from, sizeof from, text, sizeof text)) {
            int n = (int)(sizeof g->chat_log / sizeof g->chat_log[0]);
            char line[192];
            snprintf(line, sizeof line, "%s: %s", from, text);
            for (int i = 0; i < n - 1; i++)
                memcpy(g->chat_log[i], g->chat_log[i + 1], sizeof g->chat_log[i]);
            snprintf(g->chat_log[n - 1], sizeof g->chat_log[n - 1], "%s", line);
            if (g->chat_log_n < n) g->chat_log_n++;
        }
    }

    if (online_state(g->online) == ONLINE_CLOSED) {
        if (online_reconnecting(g->online)) {
            set_msg(g, "Reconnecting ...", NULL);
        } else {
            set_msg(g, "Disconnected", NULL);
            go_to_menu(g);
        }
        return;
    }
    if (online_state(g->online) == ONLINE_CONNECTING &&
        online_reconnecting(g->online)) {
        set_msg(g, "Reconnecting ...", NULL);
        return;
    }

    while (g->net_sent_ply < g->ply) {
        char u[8];
        move_to_uci(g->history[g->net_sent_ply], u);
        online_send_move(g->online, u, g->net_sent_ply);
        g->net_sent_ply++;
    }
}

static void online_rects(const Gui *g, SDL_Rect *url, SDL_Rect *time_rc,
                         SDL_Rect *code, SDL_Rect *prim, SDL_Rect *sec,
                         SDL_Rect *spec, SDL_Rect *back)
{
    int w = 460;
    int x = g->win_w / 2 - w / 2;
    url->x = x; url->y = 220; url->w = w; url->h = 44;
    time_rc->x = x; time_rc->y = 282; time_rc->w = w; time_rc->h = 40;
    code->x = x; code->y = 344; code->w = w - 140; code->h = 44;
    prim->x = g->win_w / 2 - 250; prim->y = 420; prim->w = 220; prim->h = 48;
    sec->x  = g->win_w / 2 + 30;  sec->y  = 420; sec->w = 220; sec->h = 48;
    spec->x = g->win_w / 2 - 110; spec->y = 492; spec->w = 220; spec->h = 44;
    back->x = g->win_w / 2 - 75;  back->y = 560; back->w = 150; back->h = 44;
}

static void online_cycle_focus(Gui *g, int dir)
{
    int n = online_focus_max(g) + 1;
    g->online_focus = (g->online_focus + dir + n) % n;
}

static void handle_account_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool logged = g->account_local ||
                  (g->online && online_logged_in(g->online));
    if (k == SDLK_ESCAPE) { g->account_open = false; return; }
    if (logged) {
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            if (g->online) online_logout(g->online);
            g->account_token[0]=g->account_user[0]=0;g->config_dirty=true;
            g->account_local = false;
            g->account_open = false;
        }
        return;
    }
    if (k == SDLK_TAB || k == SDLK_UP || k == SDLK_DOWN) { g->account_focus ^= 1; return; }
    if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
        char *d = g->account_focus == 0 ? g->account_user_in : g->account_pass_in;
        size_t n = strlen(d);
        if (n) d[n - 1] = '\0';
        return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { account_submit(g); return; }
}

static void handle_online_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (g->account_open) { handle_account_keydown(g, ke); return; }

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE) { online_back(g); return; }
    if (k == SDLK_l) { open_account(g); return; }
    if (k == SDLK_o) {
        if (g->online && online_logged_in(g->online)) online_logout(g->online);
        return;
    }
    if (k == SDLK_r) {
        g->online_rated = !g->online_rated;
        if (g->online) online_set_rated(g->online, g->online_rated);
        set_msg(g, g->online_rated ? "Rated game" : "Casual game", NULL);
        return;
    }
    if (k == SDLK_TAB || k == SDLK_DOWN) { online_cycle_focus(g, 1); return; }
    if (k == SDLK_UP) { online_cycle_focus(g, -1); return; }

    if (k == SDLK_LEFT) { online_cycle_time(g, -1); return; }
    if (k == SDLK_RIGHT) { online_cycle_time(g, 1); return; }

    if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
        char *dst = (g->online_focus == 0) ? g->online_url
                  : (g->online_ui == 0 && g->online_focus == 2) ? g->online_code_in
                  : NULL;
        if (dst) { size_t n = strlen(dst); if (n) dst[n - 1] = '\0'; }
        return;
    }

    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
        int f = g->online_focus;
        if (f == 1) { online_cycle_time(g, 1); return; }
        if (g->online_ui == 1) {
            if (f == 2) online_primary(g);
            else if (f == 3) online_secondary(g);
            else if (f == 4) online_back(g);
            else online_cycle_focus(g, 1);
        } else {
            if (f == 3) online_primary(g);
            else if (f == 4) online_secondary(g);
            else if (f == 5) online_spectate_action(g);
            else if (f == 6) online_back(g);
            else online_cycle_focus(g, 1);
        }
    }
}

static void handle_account_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    if (g->account_local || (g->online && online_logged_in(g->online))) return;
    char *d = g->account_focus == 0 ? g->account_user_in : g->account_pass_in;
    size_t cap = g->account_focus == 0 ? sizeof g->account_user_in
                                       : sizeof g->account_pass_in;
    size_t len = strlen(d);
    for (const char *p = te->text; *p; p++) {
        if (len + 1 >= cap) break;
        if (*p >= 0x21 && *p < 0x7f) d[len++] = *p;
    }
    d[len] = '\0';
}

static void handle_online_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    if (g->account_open) { handle_account_textinput(g, te); return; }

    char *dst = NULL;
    size_t cap = 0;
    int room_field = (g->online_ui == 0 && g->online_focus == 2);
    if (g->online_focus == 0) { dst = g->online_url; cap = sizeof g->online_url; }
    else if (room_field) { dst = g->online_code_in; cap = sizeof g->online_code_in; }
    if (!dst) return;

    size_t len = strlen(dst);
    for (const char *p = te->text; *p; p++) {
        if (len + 1 >= cap) break;
        char c = *p;
        bool ok = room_field
                ? (isalnum((unsigned char)c) != 0)
                : (c >= 0x21 && c < 0x7f);
        if (room_field) c = (char)toupper((unsigned char)c);
        if (ok) dst[len++] = c;
    }
    dst[len] = '\0';
}

static void handle_account_mousedown(Gui *g)
{
    SDL_Point p = g->mouse;
    SDL_Rect box, user, pass, btn[3];
    account_rects(g, &box, &user, &pass, btn);
    bool logged = g->account_local || (g->online && online_logged_in(g->online));
    if (logged) {
        if (pt_in(&btn[0], p.x, p.y)) {
            if (g->online) online_logout(g->online);
            g->account_token[0]=g->account_user[0]=0;g->config_dirty=true;
            g->account_local = false;
            g->account_open = false;
            return;
        }
        if (pt_in(&btn[2], p.x, p.y)) { g->account_open = false; return; }
        return;
    }
    if (pt_in(&user, p.x, p.y)) { g->account_focus = 0; return; }
    if (pt_in(&pass, p.x, p.y)) { g->account_focus = 1; return; }
    if (pt_in(&btn[0], p.x, p.y)) { account_submit(g); return; }
    if (pt_in(&btn[1], p.x, p.y)) {
        g->account_register = !g->account_register;
        g->account_focus = 0;
        return;
    }
    if (pt_in(&btn[2], p.x, p.y)) { g->account_open = false; return; }
}

static void handle_online_mousedown(Gui *g)
{
    SDL_Point p = g->mouse;

    if (g->account_open) { handle_account_mousedown(g); return; }

    SDL_Rect url, time_rc, code, prim, sec, spec, back;
    online_rects(g, &url, &time_rc, &code, &prim, &sec, &spec, &back);

    if (pt_in(&url, p.x, p.y)) { g->online_focus = 0; return; }
    if (pt_in(&time_rc, p.x, p.y)) { g->online_focus = 1; online_cycle_time(g, 1); return; }
    if (g->online_ui == 0) {
        if (pt_in(&code, p.x, p.y)) { g->online_focus = 2; return; }
        if (pt_in(&prim, p.x, p.y)) { g->online_focus = 3; online_primary(g); return; }
        if (pt_in(&sec, p.x, p.y)) { g->online_focus = 4; online_secondary(g); return; }
        if (pt_in(&spec, p.x, p.y)) { g->online_focus = 5; online_spectate_action(g); return; }
        if (pt_in(&back, p.x, p.y)) { g->online_focus = 6; online_back(g); return; }
    } else {
        if (pt_in(&prim, p.x, p.y)) { g->online_focus = 2; online_primary(g); return; }
        if (pt_in(&sec, p.x, p.y)) { g->online_focus = 3; online_secondary(g); return; }
        if (pt_in(&back, p.x, p.y)) { g->online_focus = 4; online_back(g); return; }
    }
}

/* ---- account modal ---- */

static void account_rects(const Gui *g, SDL_Rect *box, SDL_Rect *user,
                          SDL_Rect *pass, SDL_Rect btn[3])
{
    int bw = 460, bh = 280;
    int x = (g->win_w - bw) / 2, y = (g->win_h - bh) / 2;
    *box  = (SDL_Rect){ x, y, bw, bh };
    *user = (SDL_Rect){ x + 30, y + 72, bw - 60, 40 };
    *pass = (SDL_Rect){ x + 30, y + 138, bw - 60, 40 };
    int sw = 130, gap = 14, total = 3 * sw + 2 * gap;
    int sx = x + (bw - total) / 2, by = y + bh - 58;
    for (int i = 0; i < 3; i++)
        btn[i] = (SDL_Rect){ sx + i * (sw + gap), by, sw, 40 };
}

static void open_account(Gui *g)
{
    g->account_open = true;
    g->account_register = false;
    g->account_focus = 0;
    g->account_user_in[0] = '\0';
    g->account_pass_in[0] = '\0';
    if (g->account_user[0])
        snprintf(g->account_user_in, sizeof g->account_user_in, "%s", g->account_user);
}

static void account_submit(Gui *g)
{
    /* Offline: create/activate a purely local profile. */
    if (g->offline_mode || !net_ws_available()) {
        const char *name = g->account_user_in[0] ? g->account_user_in
                         : g->account_pass_in[0] ? g->account_pass_in : "Player";
        snprintf(g->local_profile, sizeof g->local_profile, "%s", name);
        snprintf(g->online_nick, sizeof g->online_nick, "%s", name);
        snprintf(g->account_user, sizeof g->account_user, "%s", name);
        g->account_local = true;
        g->account_open = false;
        g->config_dirty = true;
        gui_save_config(g);
        set_msg(g, "Local profile created", NULL);
        return;
    }
    if (!online_ensure(g)) {
        snprintf(g->menu_msg, sizeof g->menu_msg, "Not connected");
        return;
    }
    if (g->account_register)
        online_register(g->online, g->account_user_in, g->account_pass_in);
    else
        online_login(g->online, g->account_user_in, g->account_pass_in);
}

static void render_account(Gui *g, SDL_Renderer *ren)
{
    if (!g->account_open) return;
    SDL_Rect box, user, pass, btn[3];
    account_rects(g, &box, &user, &pass, btn);

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
    SDL_Rect full = { 0, 0, g->win_w, g->win_h };
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    draw_rect(ren, &box, 40, 44, 52, true);
    draw_rect(ren, &box, 120, 130, 150, false);

    bool logged = g->account_local || (g->online && online_logged_in(g->online));
    if (logged) {
        render_text(g, g->font_ui,
                    g->account_local ? "Local profile" : "Account",
                    box.x + 20, box.y + 16,
                    (SDL_Color){ 235, 235, 235, 255 });
        char line[128];
        const char *uname = g->account_local
            ? (g->local_profile[0] ? g->local_profile : g->online_nick)
            : online_username(g->online);
        snprintf(line, sizeof line, "Logged in as %s", uname);
        render_text(g, g->font_ui, line, box.x + 30, box.y + 78,
                    (SDL_Color){ 220, 230, 240, 255 });
        if (g->account_local) {
            snprintf(line, sizeof line, "Local rating %d      Puzzle %d",
                     g->local_rating, g->puzzle_rating);
        } else {
            snprintf(line, sizeof line, "PvP %d      Puzzle %d",
                     online_pvp_rating(g->online), online_puzzle_rating(g->online));
        }
        render_text(g, g->font_small, line, box.x + 30, box.y + 116,
                    (SDL_Color){ 150, 210, 230, 255 });
        snprintf(line, sizeof line, "%s",
                 g->offline_mode ? "Offline mode" : "Online mode");
        render_text(g, g->font_small, line, box.x + 30, box.y + 142,
                    (SDL_Color){ 150, 160, 175, 255 });

        draw_rect(ren, &btn[0], 55, 90, 60, true);
        draw_rect(ren, &btn[0], 120, 120, 130, false);
        render_text_centered(g, g->font_small, "Log out",
                             btn[0].x + btn[0].w / 2, btn[0].y + 11,
                             (SDL_Color){ 235, 235, 235, 255 });
        draw_rect(ren, &btn[2], 45, 45, 50, true);
        draw_rect(ren, &btn[2], 120, 120, 130, false);
        render_text_centered(g, g->font_small, "Close",
                             btn[2].x + btn[2].w / 2, btn[2].y + 11,
                             (SDL_Color){ 235, 235, 235, 255 });
        return;
    }

    render_text(g, g->font_ui,
                g->account_register ? "Create account" : "Log in",
                box.x + 20, box.y + 16, (SDL_Color){ 235, 235, 235, 255 });
    render_text(g, g->font_small, "User", user.x, user.y - 20,
                (SDL_Color){ 180, 180, 190, 255 });
    draw_rect(ren, &user, 30, 30, 32, true);
    draw_rect(ren, &user, g->account_focus == 0 ? 90 : 60,
              g->account_focus == 0 ? 140 : 60, g->account_focus == 0 ? 200 : 70, false);
    render_text(g, g->font_small, g->account_user_in, user.x + 8, user.y + 11,
                (SDL_Color){ 225, 225, 230, 255 });

    render_text(g, g->font_small, "Password", pass.x, pass.y - 20,
                (SDL_Color){ 180, 180, 190, 255 });
    draw_rect(ren, &pass, 30, 30, 32, true);
    draw_rect(ren, &pass, g->account_focus == 1 ? 90 : 60,
              g->account_focus == 1 ? 140 : 60, g->account_focus == 1 ? 200 : 70, false);
    char hidden[32];
    int n = (int)strlen(g->account_pass_in);
    if (n > 31) n = 31;
    for (int i = 0; i < n; i++) hidden[i] = '*';
    hidden[n] = '\0';
    render_text(g, g->font_small, hidden, pass.x + 8, pass.y + 11,
                (SDL_Color){ 225, 225, 230, 255 });

    char local[96];
    snprintf(local, sizeof local, "Offline: %s   puzzle %d",
             g->online_nick[0] ? g->online_nick : "Player", g->puzzle_rating);
    render_text(g, g->font_small, local, box.x + 30, box.y + 178,
                (SDL_Color){ 150, 160, 175, 255 });

    const char *l0 = g->account_register ? "Register" : "Log in";
    const char *l1 = g->account_register ? "Have an account" : "Create account";
    const char *labels[3] = { l0, l1, "Cancel" };
    for (int i = 0; i < 3; i++) {
        draw_rect(ren, &btn[i], 50, 55, 65, true);
        draw_rect(ren, &btn[i], 120, 120, 130, false);
        render_text_centered(g, g->font_small, labels[i],
                             btn[i].x + btn[i].w / 2, btn[i].y + 11,
                             (SDL_Color){ 235, 235, 235, 255 });
    }
}

static void online_status_text(const Gui *g, char *buf, size_t n)
{
    if (!g->online) { snprintf(buf, n, "Not connected"); return; }
    switch (online_state(g->online)) {
    case ONLINE_CONNECTING: snprintf(buf, n, "Connecting to %s ...", g->online_url); break;
    case ONLINE_IDLE:
        snprintf(buf, n, g->online_ui == 1 ? "Connected - ready to find a game"
                                           : "Connected - create or join a room");
        break;
    case ONLINE_HOSTING:
        snprintf(buf, n, "Room code: %s  (share it, waiting for opponent)",
                 online_room_code(g->online));
        break;
    case ONLINE_QUEUED: snprintf(buf, n, "Searching for an opponent ..."); break;
    case ONLINE_PLAYING: snprintf(buf, n, "Game starting ..."); break;
    case ONLINE_CLOSED:
        snprintf(buf, n, "Connection failed: %s", online_last_error(g->online));
        break;
    default: buf[0] = '\0'; break;
    }

    if (online_has_clocks(g->online) && online_state(g->online) != ONLINE_CLOSED) {
        int w, b;
        online_clocks(g->online, &w, &b);
        char extra[48];
        snprintf(extra, sizeof extra, "    W %d:%02d  B %d:%02d",
                 w / 60000, (w / 1000) % 60, b / 60000, (b / 1000) % 60);
        size_t len = strlen(buf);
        if (len + strlen(extra) < n) strcat(buf, extra);
    }

    char acc[112];
    if (g->online && online_logged_in(g->online))
        snprintf(acc, sizeof acc, "    %s  PvP %d / Puzzle %d%s",
                 online_username(g->online), online_pvp_rating(g->online),
                 online_puzzle_rating(g->online),
                 g->online_rated ? "  [Rated]" : "");
    else
        snprintf(acc, sizeof acc, "    not logged in    L login    %s",
                 g->online_rated ? "[Rated]" : "[Casual]");
    size_t len = strlen(buf);
    if (len + strlen(acc) < n) strcat(buf, acc);
}

static void render_online(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);

    render_text_centered(g, g->font_ui,
                         g->online_ui == 1 ? "Online Matchmaking" : "Online Multiplayer",
                         g->win_w / 2, 140, (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect url, time_rc, code, prim, sec, spec, back;
    online_rects(g, &url, &time_rc, &code, &prim, &sec, &spec, &back);

    render_text(g, g->font_small, "Server", url.x, url.y - 20,
                (SDL_Color){ 180, 180, 190, 255 });
    draw_rect(ren, &url, 30, 30, 32, true);
    draw_rect(ren, &url, g->online_focus == 0 ? 90 : 60,
              g->online_focus == 0 ? 140 : 60, g->online_focus == 0 ? 200 : 70, false);
    render_text(g, g->font_small, g->online_url, url.x + 8, url.y + 12,
                (SDL_Color){ 225, 225, 230, 255 });

    char time_label[64];
    snprintf(time_label, sizeof time_label, "Time control: %s    (click / arrows)",
             ONLINE_TIMES[g->online_time_idx].label);
    draw_rect(ren, &time_rc, 30, 30, 32, true);
    draw_rect(ren, &time_rc, g->online_focus == 1 ? 90 : 60,
              g->online_focus == 1 ? 140 : 60, g->online_focus == 1 ? 200 : 70, false);
    render_text(g, g->font_small, time_label, time_rc.x + 10, time_rc.y + 11,
                (SDL_Color){ 220, 220, 225, 255 });

    if (g->online_ui == 0) {
        render_text(g, g->font_small, "Room code (to join)", code.x, code.y - 20,
                    (SDL_Color){ 180, 180, 190, 255 });
        draw_rect(ren, &code, 30, 30, 32, true);
        draw_rect(ren, &code, g->online_focus == 2 ? 90 : 60,
                  g->online_focus == 2 ? 140 : 60, g->online_focus == 2 ? 200 : 70, false);
        render_text(g, g->font_ui, g->online_code_in[0] ? g->online_code_in : "-",
                    code.x + 10, code.y + 8, (SDL_Color){ 235, 235, 235, 255 });
    }

    const char *pl = g->online_ui == 1 ? "Find opponent" : "Create room";
    const char *sl = g->online_ui == 1 ? "Cancel" : "Join room";
    draw_rect(ren, &prim, 55, 90, 60, true);
    draw_rect(ren, &prim, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, pl, prim.x + prim.w / 2, prim.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });
    draw_rect(ren, &sec, 50, 55, 65, true);
    draw_rect(ren, &sec, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, sl, sec.x + sec.w / 2, sec.y + 10,
                         (SDL_Color){ 235, 235, 235, 255 });

    if (g->online_ui == 0) {
        draw_rect(ren, &spec, 50, 55, 65, true);
        draw_rect(ren, &spec, g->online_focus == 5 ? 120 : 100,
                  g->online_focus == 5 ? 120 : 100,
                  g->online_focus == 5 ? 130 : 110, false);
        render_text_centered(g, g->font_ui, "Spectate",
                             spec.x + spec.w / 2, spec.y + 8,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    draw_rect(ren, &back, 45, 45, 50, true);
    draw_rect(ren, &back, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, "Back", back.x + back.w / 2, back.y + 8,
                         (SDL_Color){ 235, 235, 235, 255 });

    char status[192];
    online_status_text(g, status, sizeof status);
    render_text_centered(g, g->font_small, status, g->win_w / 2, 612,
                         (SDL_Color){ 150, 210, 230, 255 });
    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, g->win_w / 2, 630,
                             (SDL_Color){ 255, 170, 120, 255 });



    render_account(g, ren);
}

/* ---- appearance picker ---- */

#define APE_COLS 6
#define APE_ROWS 3
#define APE_PER_PAGE (APE_COLS * APE_ROWS)

static void ape_tab_rect(const Gui *g, int i, SDL_Rect *r)
{
    int tw = 170, th = 40, gap = 20;
    int x0 = 208+(g->win_w-208-(4*tw+3*gap))/2;
    r->x = x0 + i * (tw + gap);
    r->y = 96;
    r->w = tw;
    r->h = th;
}

static void ape_cell_rect(const Gui *g, int j, SDL_Rect *r)
{
    int cw = 170, ch = 150, gap = 12;
    int x0 = 208 + (g->win_w - 208 - (APE_COLS * cw + (APE_COLS - 1) * gap)) / 2;
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

static void ape_page_rect(const Gui *g, int direction, SDL_Rect *r)
{
    int center = 208 + (g->win_w - 208) / 2;
    *r = (SDL_Rect){ center + (direction < 0 ? -310 : 174), 654, 136, 38 };
}

static int appearance_count(Gui *g, int tab)
{
    if (tab == 0) return g->boards.count;
    if (tab == 1) return g->pieces.count;
    return tab==2?ANIM_STYLE_COUNT:2;
}

static void open_appearance(Gui *g, Scene return_to)
{
    g->ape_return_scene = return_to;
    g->ape_tab = 0;
    g->ape_sel[0] = g->board_index;
    g->ape_sel[1] = g->piece_index;
    g->ape_sel[2] = (int)g->anim_style;
    g->ape_sel[3]=g->font_index;
    for(int i=0;i<4;i++)g->ape_page[i]=g->ape_sel[i]/APE_PER_PAGE;
    g->ape_started = SDL_GetTicks();
    g->scene = SCENE_APPEARANCE;
}

static void appearance_apply(Gui *g)
{
    switch (g->ape_tab) {
        case 0: g->board_index = g->ape_sel[0]; load_board_style(g); break;
        case 1: g->piece_index = g->ape_sel[1]; load_piece_style(g); break;
        case 2: g->anim_style = (AnimStyle)g->ape_sel[2]; clear_anims(g); break;
        default: g->font_index=g->ape_sel[3];rebuild_fonts(g);break;
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
    g->ape_page[g->ape_tab] = sel / APE_PER_PAGE;
    appearance_apply(g);
}

static void appearance_page_move(Gui *g, int direction)
{
    if(g->ape_tab>=2)return;
    int count=appearance_count(g,g->ape_tab),last=count>0?(count-1)/APE_PER_PAGE:0;
    g->ape_page[g->ape_tab]=clampi(g->ape_page[g->ape_tab]+direction,0,last);
}

static void handle_appearance_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { g->scene = g->ape_return_scene; return; }
    if (k == SDLK_PAGEUP) { appearance_page_move(g,-1); return; }
    if (k == SDLK_PAGEDOWN) { appearance_page_move(g,1); return; }
    if (k == SDLK_LEFT)  { appearance_move(g, -1); return; }
    if (k == SDLK_RIGHT) { appearance_move(g, 1); return; }
    if (k == SDLK_UP)    { appearance_move(g, -APE_COLS); return; }
    if (k == SDLK_DOWN)  { appearance_move(g, APE_COLS); return; }
    if (k == SDLK_TAB)   { g->ape_tab = (g->ape_tab + 1) % 4; return; }
    if (k >= SDLK_1 && k <= SDLK_4) {
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

    for (int i = 0; i < 4; i++) {
        SDL_Rect r;
        ape_tab_rect(g, i, &r);
        if (pt_in(&r, p.x, p.y)) { g->ape_tab = i; return; }
    }
    SDL_Rect back;
    ape_back_rect(g, &back);
    if (pt_in(&back, p.x, p.y)) { g->scene = g->ape_return_scene; return; }

    int tab = g->ape_tab;
    int n = appearance_count(g, tab);
    if(tab<2)for(int direction=-1;direction<=1;direction+=2){
        SDL_Rect r;ape_page_rect(g,direction,&r);
        if(pt_in(&r,p.x,p.y)){appearance_page_move(g,direction);return;}
    }
    int start = g->ape_page[tab] * APE_PER_PAGE;
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

void gui_ensure_engine(Gui *g)
{
    if (g->engine_install) return;
    const char *existing = ai_find_engine(g->engine_path);
    if (existing) {
        snprintf(g->engine_path, sizeof g->engine_path, "%s", existing);
        g->config_dirty = true;
        return;
    }
    g->engine_install = engine_install_start();
    snprintf(g->engine_install_status, sizeof g->engine_install_status, "%s",
             g->engine_install ? "Downloading and installing Stockfish..."
                               : "Stockfish installation failed. Click to retry.");
}

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
#ifdef _WIN32
            const char *sep = ";", *suffix = ".exe";
#else
            const char *sep = ":", *suffix = "";
#endif
            for (char *dir = strtok(dup, sep); dir && g->engine_count < ENGINE_MAX;
                 dir = strtok(NULL, sep)) {
                for (int i = 0; names[i] && g->engine_count < ENGINE_MAX; i++) {
                    char full[512];
                    snprintf(full, sizeof full, "%s/%s%s", dir, names[i], suffix);
#ifdef _WIN32
                    if (_access(full, 0) != 0) continue;
#else
                    if (access(full, X_OK) != 0) continue;
#endif
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
    custom->x = 240; custom->y = g->win_h - 150; custom->w = g->win_w - 260; custom->h = 40;
    back->w = 140; back->h = 42; back->x = g->win_w - 160; back->y = g->win_h - 90;
}

static void engine_ctrl_rects(const Gui *g,
                              SDL_Rect minus[ENG_CTRL_COUNT],
                              SDL_Rect plus[ENG_CTRL_COUNT])
{
    (void)g;
    int x = 240, y0 = 170, w = 30, h = 30, gap = 8;
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
    SDL_Rect install = {240, 380, 360, 40};
    if (!g->engine_path[0] && pt_in(&install, g->mouse.x, g->mouse.y)) {
        gui_ensure_engine(g);
        detect_engines(g);
        return;
    }
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

    const char *result = g->override_result[0]
                       ? g->override_result
                       : pgn_result(&g->board, g->state);
    if(play_mode(g))play_sync(g);
    MoveNode *export_tree=play_mode(g)?g->play_tree:g->mode==MODE_ANALYSIS?g->pgntree:NULL;
    if (export_tree) {
        PgnHeaders h = play_mode(g)?(PgnHeaders){0}:g->pgn_hdr;
        if (!h.white[0]) snprintf(h.white, sizeof h.white, "%s", g->white_name);
        if (!h.black[0]) snprintf(h.black, sizeof h.black, "%s", g->black_name);
        if (!h.result[0]) snprintf(h.result, sizeof h.result, "%s", result);
        char *text = pgn_serialize(export_tree, &h);
        if (text) { fputs(text, f); free(text); }
    } else {
        pgn_write(f, (const char (*)[8])g->move_san, g->ply,
                  "OpenChess", "OpenChess", date, 1,
                  g->white_name, g->black_name, result);
    }
    fclose(f);

    g->pgn_prompt = false;
    set_msg(g, "Saved PGN: %s", path);
}

/* ---- PGN import pop-up (analysis) ---- */

static void pgn_import_rects(const Gui *g, SDL_Rect *box, SDL_Rect *text,
                             SDL_Rect btn[4])
{
    int bw = 700, bh = 440;
    int x = (g->win_w - bw) / 2, y = 110;
    *box  = (SDL_Rect){ x, y, bw, bh };
    *text = (SDL_Rect){ x + 20, y + 52, bw - 40, bh - 130 };
    int sw = 140, gap = 16, total = 4 * sw + 3 * gap;
    int sx = x + (bw - total) / 2, by = y + bh - 60;
    for (int i = 0; i < 4; i++)
        btn[i] = (SDL_Rect){ sx + i * (sw + gap), by, sw, 44 };
}

static void pgn_import_open(Gui *g)
{
    g->pgn_import_open = true;
    g->pgn_text_len = 0;
    g->pgn_text[0] = '\0';
    g->pgn_scroll = 0;
    g->pgn_scroll_max = 0;
    san_close_box(g);
}

static void pgn_import_close(Gui *g)
{
    g->pgn_import_open = false;
    g->pgn_text_len = 0;
    g->pgn_text[0] = '\0';
    g->pgn_scroll = 0;
    g->pgn_scroll_max = 0;
}

static void pgn_import_paste(Gui *g)
{
    char *clip = SDL_GetClipboardText();
    if (clip) {
        snprintf(g->pgn_text, sizeof g->pgn_text, "%s", clip);
        g->pgn_text_len = (int)strlen(g->pgn_text);
        g->pgn_scroll = 0;
        SDL_free(clip);
    }
}

/* Native "open file" dialog; returns the chosen path or false. Uses the OS
 * helper available on each platform (no bundled dependency). */
static bool pick_pgn_file(char *out, size_t n)
{
    out[0] = '\0';
#if defined(__APPLE__)
    FILE *p = popen("osascript -e 'POSIX path of (choose file with prompt "
                    "\"Open PGN or FEN\")' 2>/dev/null", "r");
#elif defined(__linux__)
    FILE *p = popen("zenity --file-selection --title='Open PGN' "
                    "--file-filter='Chess files | *.pgn *.fen *.txt' 2>/dev/null", "r");
#elif defined(_WIN32)
    FILE *p = popen("powershell -NoProfile -Command \"Add-Type -AssemblyName "
                    "System.Windows.Forms; $d=New-Object "
                    "System.Windows.Forms.OpenFileDialog; "
                    "$d.Filter='PGN or FEN|*.pgn;*.fen;*.txt'; "
                    "if($d.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK)"
                    "{$d.FileName}\" 2>NUL", "r");
#else
    FILE *p = NULL;
#endif
    if (!p) return false;
    bool ok = fgets(out, (int)n, p) != NULL;
    pclose(p);
    if (!ok) return false;
    size_t l = strlen(out);
    while (l && (out[l - 1] == '\n' || out[l - 1] == '\r')) out[--l] = '\0';
    return out[0] != '\0';
}

static bool pgn_import_read_file(Gui *g, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { set_msg(g, "Could not read PGN", NULL); return false; }
    size_t n = fread(g->pgn_text, 1, sizeof g->pgn_text - 1, f);
    g->pgn_text[n] = '\0';
    g->pgn_text_len = (int)n;
    g->pgn_scroll = 0;
    fclose(f);
    return true;
}

/* Upload: native file dialog; falls back to the first .pgn in the games dir. */
static void pgn_import_upload(Gui *g)
{
    char path[1200];
    if (pick_pgn_file(path, sizeof path)) {
        pgn_import_read_file(g, path);
        return;
    }

    DIR *d = opendir(path_games_dir());
    if (!d) { set_msg(g, "Could not open a file dialog", NULL); return; }
    char found[1200] = "";
    struct dirent *e;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 4 && strcasecmp(e->d_name + n - 4, ".pgn") == 0) {
            snprintf(found, sizeof found, "%s/%s", path_games_dir(), e->d_name);
            break;
        }
    }
    closedir(d);
    if (!found[0]) { set_msg(g, "No .pgn selected", NULL); return; }
    pgn_import_read_file(g, found);
}

static void pgn_import_load(Gui *g)
{
    PgnHeaders h;
    MoveNode *root = pgn_parse_checked(g->pgn_text, &h);
    if(!root){if(g->ui)snprintf(g->ui->status,sizeof g->ui->status,"%s",ui_tr(g,"PGN contains invalid moves, FEN or variations.","PGN 包含无效走子、FEN 或变例，请修改后重试。"));set_msg(g,"Invalid PGN",NULL);return;}

    char validation[160];if(!position_validate(&root->board,validation)){if(g->ui)snprintf(g->ui->status,sizeof g->ui->status,"%s",validation);mt_free(root);return;}
    if (!root->first && !strstr(g->pgn_text,"[FEN ")) {if(g->ui)snprintf(g->ui->status,sizeof g->ui->status,"%s",ui_tr(g,"No moves parsed.","未解析到有效走子。"));set_msg(g, "No moves parsed", NULL); mt_free(root); return; }

    /* Always clear the previous analysis tree/board and replace it. */
    if (g->pgntree) { mt_free(g->pgntree); g->pgntree = NULL; }
    g->pgntree = root;
    g->pgn_hdr = h;

    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    g->flipped = false;
    g->auto_flip = false;
    g->tree_cur = g->pgntree;
    while (g->tree_cur->first) g->tree_cur = g->tree_cur->first;  /* end of mainline */
    for (int i = 0; i < MAX_PLY; i++) {
        g->review_cls[i] = RC_NONE;
        g->review_cpl[i] = 0;
        g->review_acc[i] = 0.0;
        g->move_was_best[i] = false;
    }
    g->review_on = false;
    g->rev_report = false;
    g->rev_accuracy = 0.0;
    g->rev_acpl = 0;
    g->rev_moves = 0;
    memset(g->rev_count, 0, sizeof g->rev_count);
    memset(g->rev_count_side, 0, sizeof g->rev_count_side);
    memset(g->rev_accuracy_side, 0, sizeof g->rev_accuracy_side);
    memset(g->rev_acpl_side, 0, sizeof g->rev_acpl_side);
    memset(g->rev_moves_side, 0, sizeof g->rev_moves_side);
    g->opening_cur_valid = false;
    g->opening_cur_node = NULL;
    snprintf(g->white_name, sizeof g->white_name, "%s", h.white[0] ? h.white : "White");
    snprintf(g->black_name, sizeof g->black_name, "%s", h.black[0] ? h.black : "Black");
    g->saved.valid = false;
    g->override_result[0] = 0;
    sync_from_tree(g);
    analysis_refresh(g);
    pgn_import_close(g);
    /* Review starts only when the user clicks Analyze (V). */
    set_msg(g, "PGN loaded - click Analyze to review", NULL);
}

/* Draw `text` word-wrapped inside `box`, offset by `scroll`. Returns the number
 * of content lines (for scrolling). */
static int render_wrapped_text(Gui *g, SDL_Renderer *ren, const char *text,
                               SDL_Rect box, int scroll, SDL_Color col)
{
    (void)ren;
    int line_h = text_height(g, g->font_small) + 3;
    int maxw = box.w - 16;
    int x = box.x + 8;
    int y = box.y + 8 - scroll;
    int total = 0;
    char cur[320];
    int cl = 0;
    cur[0] = '\0';

    const char *p = text;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char w[128];
        int n = 0;
        while (*p && !isspace((unsigned char)*p) && n < 127) w[n++] = *p++;
        w[n] = '\0';

        char trial[460];
        if (cl) snprintf(trial, sizeof trial, "%s %s", cur, w);
        else    snprintf(trial, sizeof trial, "%s", w);

        if (cl && mv_width(g, trial) > maxw) {
            if (y + line_h > box.y && y < box.y + box.h)
                render_text(g, g->font_small, cur, x, y, col);
            total++;
            y += line_h;
            snprintf(cur, sizeof cur, "%s", w);
            cl = (int)strlen(cur);
        } else {
            snprintf(cur, sizeof cur, "%s", trial);
            cl = (int)strlen(cur);
        }
    }
    if (cl) {
        if (y + line_h > box.y && y < box.y + box.h)
            render_text(g, g->font_small, cur, x, y, col);
        total++;
    }
    return total;
}

static void render_pgn_import(Gui *g, SDL_Renderer *ren)
{
    if (!g->pgn_import_open) return;
    SDL_Rect box, text, btn[4];
    pgn_import_rects(g, &box, &text, btn);

    set_render_color(ren, 0, 0, 0);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_Rect full = { 0, 0, g->win_w, g->win_h };
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 150);
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    draw_rect(ren, &box, 40, 44, 52, true);
    draw_rect(ren, &box, 120, 130, 150, false);
    render_text(g, g->font_ui, "Import PGN", box.x + 20, box.y + 14,
                (SDL_Color){ 235, 235, 235, 255 });

    draw_rect(ren, &text, 25, 25, 30, true);
    draw_rect(ren, &text, 90, 140, 200, false);
    SDL_RenderSetClipRect(ren, &text);
    if (g->pgn_text_len) {
        int lines = render_wrapped_text(g, ren, g->pgn_text, text, g->pgn_scroll,
                                        (SDL_Color){ 225, 225, 230, 255 });
        int line_h = text_height(g, g->font_small) + 3;
        int content_h = 16 + lines * line_h;
        g->pgn_scroll_max = content_h > text.h ? content_h - text.h : 0;
        if (g->pgn_scroll > g->pgn_scroll_max) g->pgn_scroll = g->pgn_scroll_max;
        if (g->pgn_scroll < 0) g->pgn_scroll = 0;
    } else {
        g->pgn_scroll_max = 0;
        render_text(g, g->font_small,
                    "Paste PGN here, or choose a file.", text.x + 8, text.y + 8,
                    (SDL_Color){ 120, 120, 128, 255 });
    }
    SDL_RenderSetClipRect(ren, NULL);

    const char *labels[4] = { "Paste", "Upload", "Load", "Cancel" };
    for (int i = 0; i < 4; i++) {
        draw_rect(ren, &btn[i], 50, 55, 65, true);
        draw_rect(ren, &btn[i], 120, 120, 130, false);
        render_text_centered(g, g->font_ui, labels[i],
                             btn[i].x + btn[i].w / 2, btn[i].y + 10,
                             (SDL_Color){ 235, 235, 235, 255 });
    }
}

static void handle_game_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (g->pgn_import_open) {
        if (k == SDLK_ESCAPE) { pgn_import_close(g); return; }
        if (ctrl && k == SDLK_v) { pgn_import_paste(g); return; }
        if (k == SDLK_UP) {
            g->pgn_scroll -= 26;
            if (g->pgn_scroll < 0) g->pgn_scroll = 0;
            return;
        }
        if (k == SDLK_DOWN) {
            g->pgn_scroll += 26;
            if (g->pgn_scroll > g->pgn_scroll_max) g->pgn_scroll = g->pgn_scroll_max;
            return;
        }
        if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            if (g->pgn_text_len > 0) g->pgn_text[--g->pgn_text_len] = '\0';
            return;
        }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { pgn_import_load(g); return; }
        return;
    }
    if (g->mode == MODE_ANALYSIS && ctrl && k == SDLK_o) { pgn_import_open(g); return; }

    if (g->pgn_prompt) {
        if (k == SDLK_ESCAPE) { g->pgn_prompt = false; return; }
        if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            if (g->pgn_len > 0) g->pgn_name[--g->pgn_len] = '\0';
            return;
        }
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) pgn_save(g);
        return;
    }

    if (g->chat_open) {
        if (k == SDLK_ESCAPE) { g->chat_open = false; g->chat_len = 0; }
        else if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
            if (g->chat_len > 0) g->chat_text[--g->chat_len] = '\0';
        } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            g->chat_text[g->chat_len] = '\0';
            if (g->chat_len > 0 && g->online) online_send_chat(g->online, g->chat_text);
            g->chat_open = false;
            g->chat_len = 0;
        }
        return;
    }

    /* T opens the online chat line. */
    if (g->mode == MODE_ONLINE && !ctrl && k == SDLK_t) {
        g->chat_open = true;
        g->chat_len = 0;
        g->chat_text[0] = '\0';
        san_close_box(g);
        return;
    }

    if (g->mode == MODE_ONLINE && ctrl && k == SDLK_a) {
        if (g->draw_offered && g->online) {
            online_draw_accept(g->online);
            g->draw_offered = false;
        }
        return;
    }
    if (g->mode == MODE_ONLINE && ctrl && k == SDLK_d) {
        if (g->draw_offered && g->online) {
            online_draw_decline(g->online);
            g->draw_offered = false;
        } else if (g->online && !online_spectating(g->online) && !g->online_over) {
            online_draw_offer(g->online);
            set_msg(g, "Draw offered", NULL);
        }
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
        if (g->mode == MODE_PUZZLE) puzzle_hint(g);
        else if (g->mode == MODE_LOCAL || g->mode == MODE_ONLINE)
            set_msg(g, "Undo disabled in multiplayer", NULL);
        else do_undo(g);
        return;
    }
    if(net_is_p2p(g->net)&&ctrl&&(k==SDLK_d||k==SDLK_a)){p2p_draw(g,k==SDLK_a);return;}
    if (ctrl && k == SDLK_r) {
        if(net_is_p2p(g->net)){p2p_action(g);return;}
        if (g->mode == MODE_PUZZLE) {
            open_puzzle(g);          /* next puzzle */
        } else if (g->mode == MODE_ONLINE) {
            if (g->online && g->online_over) online_rematch(g->online);
            else set_msg(g, "Restart disabled online", NULL);
        } else if (g->mode == MODE_LOCAL) {
            set_msg(g, "Restart disabled in multiplayer", NULL);
        } else {
            do_restart(g);
        }
        return;
    }

    if (g->mode == MODE_PUZZLE && !ctrl) {
        if (k == SDLK_h) { puzzle_hint(g); return; }
        if (k == SDLK_r) { puzzle_reveal(g); return; }
        if (k == SDLK_n) { open_puzzle(g); return; }
    }
    if (g->mode == MODE_ANALYSIS && !ctrl && k == SDLK_v) {
        if (g->rev_report && !g->review_on) open_review_report(g);
        else                                review_start(g);
        return;
    }
    if(!ctrl&&!g->san_open&&!g->fen_active&&(k==SDLK_LEFT||k==SDLK_RIGHT||k==SDLK_UP||k==SDLK_DOWN)){
        history_navigate(g,k==SDLK_LEFT?-1:k==SDLK_RIGHT?1:k==SDLK_UP?-2:2);return;
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

    /* Space opens SAN input. Enter also remains available for existing users. */
    if (k == SDLK_SPACE || k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        san_open_box(g);
        return;
    }
}

static void handle_game_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    if (g->pgn_import_open) {
        for (const char *p = te->text; *p; p++) {
            if (g->pgn_text_len >= (int)sizeof g->pgn_text - 1) break;
            if (*p == '\r' || *p == '\n') continue;
            if (*p >= 0x20 && *p < 0x7f) g->pgn_text[g->pgn_text_len++] = *p;
        }
        g->pgn_text[g->pgn_text_len] = '\0';
        return;
    }

    if (g->pgn_prompt) {
        for (const char *p = te->text; *p; p++) {
            if (g->pgn_len >= (int)sizeof g->pgn_name - 1) break;
            if (*p >= 0x20 && *p < 0x7f) g->pgn_name[g->pgn_len++] = *p;
        }
        g->pgn_name[g->pgn_len] = '\0';
        return;
    }

    if (g->chat_open) {
        for (const char *p = te->text; *p; p++) {
            if (g->chat_len >= (int)sizeof g->chat_text - 1) break;
            if (*p >= 0x20 && *p < 0x7f) g->chat_text[g->chat_len++] = *p;
        }
        g->chat_text[g->chat_len] = '\0';
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
        if(*p==' '&&g->input.len==0)continue;
        if (san_char_ok(*p)) g->input.text[g->input.len++] = *p;
    }
    g->input.text[g->input.len] = 0;
}

static void analysis_nav_rect(const Gui *g, int index, SDL_Rect *r)
{
    *r = (SDL_Rect){ g->panel_x + index * (PANEL_W / 2 + 2),
                    g->board_y + (g->analysis_summary ? 582 : 384),
                    PANEL_W / 2 - 2, 28 };
}

static void analysis_tab_rect(const Gui *g, int index, SDL_Rect *r)
{
    *r = (SDL_Rect){ g->panel_x + index * (PANEL_W / 2 + 2),
                    g->board_y + 194, PANEL_W / 2 - 2, 22 };
}

static void analysis_graph_rect(const Gui *g, SDL_Rect *r)
{
    *r = (SDL_Rect){ g->panel_x, g->board_y + 414, PANEL_W, 42 };
}

static void handle_game_mousedown(Gui *g)
{
    SDL_Point p = g->mouse;
    g->dragging = false;

    if (g->pgn_prompt) return;   /* modal: keyboard only */

    if(g->history_browsing){SDL_Rect field;input_rect(g,&field);if(pt_in(&field,p.x,p.y)){history_seek(g,g->ply);return;}}
    if(g->mode!=MODE_ANALYSIS){
        for(int i=0;i<g->move_hit_count;i++)if(pt_in(&g->move_hit_rect[i],p.x,p.y)){
            int ply=g->move_hit_ply[i];if(play_mode(g)&&g->move_hit_node[i]){g->play_view=g->move_hit_node[i];g->history_cursor=mt_depth(g->play_view);g->history_browsing=g->play_view!=g->play_live;clear_selection(g);san_close_box(g);g->promo_from=g->promo_to=-1;g->dragging=false;g->move_follow=true;}else if(ply>0)history_seek(g,ply);return;
        }
        if(sq_from_pos(g,p.x,p.y)>=0){
            if(g->mode==MODE_PUZZLE){if(g->history_browsing)return;puzzle_prepare_input(g);}
        }
    }
    /* Click a previous move (or a variation) to jump to that position. */
    if (g->mode == MODE_ANALYSIS && !g->pgn_import_open) {
        for (int i = 0; i < 2; i++) {
            SDL_Rect r;
            analysis_tab_rect(g, i, &r);
            if (pt_in(&r, p.x, p.y)) {
                g->analysis_summary = i != 0;
                g->fen_active = false;
                g->move_drag = false;
                g->move_hit_count = 0;
                san_close_box(g);
                return;
            }
        }
        SDL_Rect graph;
        analysis_graph_rect(g, &graph);
        if (!g->analysis_summary && g->pgntree && pt_in(&graph, p.x, p.y)) {
            int count = 0;
            for (MoveNode *n = g->pgntree->first; n && count < MAX_PLY; n = n->first) count++;
            int step = (int)lroundf((p.x - graph.x) * (float)count / (graph.w - 1));
            MoveNode *n = g->pgntree;
            while (step-- > 0 && n->first) n = n->first;
            clear_anims(g);
            g->tree_cur = n;
            sync_from_tree(g);
            analysis_refresh(g);
            return;
        }
        for (int i = 0; i < 2; i++) {
            SDL_Rect r;
            analysis_nav_rect(g, i, &r);
            if (pt_in(&r, p.x, p.y)) {
                if (i == 0) tree_go_back(g);
                else tree_go_forward(g);
                return;
            }
        }
        for (int i = 0; !g->analysis_summary && i < g->move_hit_count; i++) {
            if (pt_in(&g->move_hit_rect[i], p.x, p.y)) {
                clear_anims(g);
                g->check_anim = 0;
                g->tree_cur = g->move_hit_node[i];
                sync_from_tree(g);
                analysis_refresh(g);
                return;
            }
        }
    }

    if (g->pgn_import_open) {
        SDL_Rect box, text, btn[4];
        pgn_import_rects(g, &box, &text, btn);
        for (int i = 0; i < 4; i++) {
            if (!pt_in(&btn[i], p.x, p.y)) continue;
            if (i == 0) pgn_import_paste(g);
            else if (i == 1) pgn_import_upload(g);
            else if (i == 2) pgn_import_load(g);
            else pgn_import_close(g);
            return;
        }
        return;
    }

    /* Drag the move list to scroll it. */
    if (!(g->mode == MODE_ANALYSIS && g->analysis_summary) &&
        pt_in(&g->move_box, p.x, p.y)) {
        g->move_drag = true;
        g->move_drag_y0 = p.y;
        g->move_drag_scroll0 = g->move_scroll;
        g->move_follow = false;
        return;
    }

    /* MultiPV slider + engine-arrow toggle (analysis). */
    if (g->mode == MODE_ANALYSIS) {
        SDL_Rect box;
        engine_arrows_rect(g, &box);
        SDL_Rect ahit = { box.x - 4, box.y - 4, box.w + 8, box.h + 8 };
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
        if (g->mode == MODE_PUZZLE) puzzle_hint(g);
        else if (g->mode == MODE_LOCAL || g->mode == MODE_ONLINE)
            set_msg(g, "Undo disabled in multiplayer", NULL);
        else do_undo(g);
        return;
    }
    if (pt_in(&restart, p.x, p.y)) {
        if(net_is_p2p(g->net)){p2p_action(g);return;}
        if (g->mode == MODE_PUZZLE) {
            puzzle_reveal(g);
        } else if (g->mode == MODE_ANALYSIS) {
            if (g->rev_report) open_review_report(g);
            else               review_start(g);
        } else if (g->mode == MODE_ONLINE) {
            if (g->online && !online_spectating(g->online)) {
                if (g->online_over) online_rematch(g->online);
                else                online_resign(g->online);
            }
        } else if (g->mode == MODE_LOCAL) {
            set_msg(g, "Restart disabled in multiplayer", NULL);
        } else {
            do_restart(g);
        }
        return;
    }
    if (pt_in(&styles, p.x, p.y)) {
        if (g->mode == MODE_PUZZLE) open_puzzle(g);
        else if (g->mode == MODE_ANALYSIS) pgn_import_open(g);
        else if(ui_finished_game(g))ui_game_analyse(g);
        return;
    }
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
    if (g->mode == MODE_ANALYSIS && g->analysis_summary &&
        p.x >= g->panel_x && p.y >= g->board_y + 218 &&
        p.y < g->board_y + 582) return;
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
        Piece choices[4] = { WQ, WN, WR, WB };
        for (int i = 0; i < 4; i++)
            if (pt_in(&pr[i], p.x, p.y)) { resolve_promotion(g, choices[i]); return; }
        /* Double click on the target square: default to the queen. */
        if (SDL_GetTicks() - g->promo_open_ms <= 350) {
            resolve_promotion(g, WQ);
            return;
        }
        g->promo_from = g->promo_to = -1;
        return;
    }

    int sq = sq_from_pos(g, p.x, p.y);
    if (sq < 0) return;
    ann_clear(g);   /* a left click on the board clears annotations */

    if (g->selected >= 0) {
        Move *m = target_move(g, sq);
        if (m) {
            g->anim_fast = false;   /* click-click move: normal slide */
            try_move_to(g, sq);
            return;
        }
    }

    /* In singleplayer/local the player may only move their own side. */
    bool may_move = true;
    if (g->mode == MODE_SINGLE)     may_move = (g->board.side == g->human_color);
    else if (g->mode == MODE_LOCAL) may_move = (g->board.side == g->local_color)&&(!g->p2p_started||(g->net&&g->p2p_ready&&!p2p_finished(g)));

    Board *position=input_board(g);if(play_branch(g))may_move=true;
    Piece pc = position->board[sq];
    bool my = (position->side == WHITE) ? WHITE_PIECE(pc) : BLACK_PIECE(pc);
    if (pc != EMPTY && my && may_move && game_state(position) == NO_GAME_OVER) {
        /* A second click dismisses the selection and legal targets. */
        select_square(g, sq);
    } else {
        clear_selection(g);
    }
}

static void handle_game_mouseup(Gui *g)
{
    SDL_Point p = g->mouse;
    bool was_drag = g->dragging;
    g->dragging = false;
    if (g->selected >= 0) {
        g->pick_released = SDL_GetTicks() + 1;
        g->pick_position = g->mouse;
    }

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
    if (g->selected >= 0) {
        g->anim_fast = was_drag;   /* a drag drop snaps with a fast slide */
        try_move_to(g, sq);
        g->anim_fast = false;
    }
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
    if(p2p_event(g,e))return;
    if(ui_event(g,e))return;
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
            if (g->move_drag) {
                g->move_scroll = g->move_drag_scroll0 - (g->mouse.y - g->move_drag_y0);
                if (g->move_scroll < 0) g->move_scroll = 0;
                if (g->move_scroll > g->move_scroll_max) g->move_scroll = g->move_scroll_max;
                return;
            }
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
                int dx = g->mouse.x - g->pick_mouse.x;
                int dy = g->mouse.y - g->pick_mouse.y;
                if (!g->dragging && g->selected >= 0 && dx * dx + dy * dy >= 16) {
                    g->dragging = true;
                    g->pick_started = SDL_GetTicks() + 1;
                    g->pick_released = 0;
                }
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
            g->pick_mouse = g->mouse;
            debug_ui_log(g, e->button.x, e->button.y);
            if (g->account_open) { handle_account_mousedown(g); return; }
            if (g->scene == SCENE_MENU) handle_menu_mousedown(g);
            else if (g->scene == SCENE_SINGLE_SETUP) handle_setup_mousedown(g);
            else if (g->scene == SCENE_PUZZLE_SETUP) handle_puzzle_setup_mousedown(g);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_mousedown(g);
            else if (g->scene == SCENE_ONLINE) handle_online_mousedown(g);
            else if (g->scene == SCENE_OPENINGS) handle_openings_mousedown(g);
            else if (g->scene == SCENE_APPEARANCE) handle_appearance_mousedown(g);
            else if (g->scene == SCENE_SETTINGS) handle_settings_mousedown(g);
            else if (g->scene == SCENE_REVIEW) handle_review_mousedown(g);
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
            if (g->scene == SCENE_GAME && g->move_drag) {
                g->move_drag = false;
                return;
            }
            if (g->scene == SCENE_GAME && g->eng_slider_drag) {
                g->eng_slider_drag = false;
                return;
            }
            if (g->scene == SCENE_GAME) handle_game_mouseup(g);
            else if (g->scene == SCENE_OPENINGS && g->selected >= 0) {
                g->pick_released = SDL_GetTicks() + 1;
                g->pick_position = g->mouse;
            }
            return;
        case SDL_MOUSEWHEEL:
            if (g->scene == SCENE_GAME && g->pgn_import_open) {
                SDL_Rect box, text, btns[4];
                pgn_import_rects(g, &box, &text, btns);
                if (pt_in(&text, g->mouse.x, g->mouse.y)) {
                    g->pgn_scroll -= e->wheel.y * 40;
                    if (g->pgn_scroll < 0) g->pgn_scroll = 0;
                    if (g->pgn_scroll > g->pgn_scroll_max)
                        g->pgn_scroll = g->pgn_scroll_max;
                }
                return;
            }
            if (g->scene == SCENE_GAME &&
                !(g->mode == MODE_ANALYSIS && g->analysis_summary) &&
                pt_in(&g->move_box, g->mouse.x, g->mouse.y)) {
                g->move_scroll -= e->wheel.y * 40;
                if (g->move_scroll < 0) g->move_scroll = 0;
                if (g->move_scroll > g->move_scroll_max)
                    g->move_scroll = g->move_scroll_max;
                g->move_follow = false;
                return;
            }
            return;
        case SDL_KEYDOWN:
            if (g->account_open) { handle_account_keydown(g, &e->key); return; }
            if (g->scene == SCENE_MENU) handle_menu_keydown(g, &e->key);
            else if (g->scene == SCENE_SINGLE_SETUP) handle_setup_keydown(g, &e->key);
            else if (g->scene == SCENE_PUZZLE_SETUP) handle_puzzle_setup_keydown(g, &e->key);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_keydown(g, &e->key);
            else if (g->scene == SCENE_ONLINE) handle_online_keydown(g, &e->key);
            else if (g->scene == SCENE_OPENINGS) handle_openings_keydown(g, &e->key);
            else if (g->scene == SCENE_APPEARANCE) handle_appearance_keydown(g, &e->key);
            else if (g->scene == SCENE_SETTINGS) handle_settings_keydown(g, &e->key);
            else if (g->scene == SCENE_REVIEW) handle_review_keydown(g, &e->key);
            else handle_game_keydown(g, &e->key);
            return;
        case SDL_TEXTINPUT:
            if (g->account_open) { handle_account_textinput(g, &e->text); return; }
            if (g->scene == SCENE_GAME) handle_game_textinput(g, &e->text);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_textinput(g, &e->text);
            else if (g->scene == SCENE_ONLINE) handle_online_textinput(g, &e->text);
            else if (g->scene == SCENE_OPENINGS) handle_openings_textinput(g, &e->text);
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
    while ((r = transport_poll(g->net, line, sizeof line)) == 1) {
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
        transport_close(g->net);
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
        transport_send(g->net, msg);
        g->net_sent_ply++;
    }
}

static void hostjoin_tick(Gui *g)
{
    if (!g->net) return;

    char line[256];
    int r;
    while ((r = transport_poll(g->net, line, sizeof line)) == 1) {
        /* ignore the peer's HELLO/FEN; sides are derived from host/join role */
    }
    if (r < 0) {
        transport_close(g->net);
        g->net = NULL;
        g->net_waiting = false;
        snprintf(g->menu_msg, sizeof g->menu_msg, "Connection closed");
        return;
    }
    if (transport_state(g->net) == NET_STATE_CONNECTED) {
        Color side = (transport_role(g->net) == NET_ROLE_HOST) ? WHITE : BLACK;
        enter_local_game(g, side);
    }
}

void gui_tick(Gui *g, Uint32 now)
{
    if (g->engine_install) {
        int result = engine_install_poll(g->engine_install);
        if (result) {
            engine_install_free(g->engine_install);
            g->engine_install = NULL;
            AiEngine *probe = result > 0 ? ai_start(path_stockfish()) : NULL;
            if (probe) {
                ai_stop(probe);
                /* A user may have chosen another engine while downloading. */
                if (!g->engine_path[0]) apply_engine(g, path_stockfish());
                detect_engines(g);
            }
            snprintf(g->engine_install_status, sizeof g->engine_install_status, "%s",
                     probe ? "Stockfish installed successfully."
                           : "Stockfish installation failed. Click to retry.");
        }
    }
    if(g->online){
        if(online_state(g->online)==ONLINE_PLAYING&&g->mode==MODE_ONLINE)online_tick(g);
        else online_lobby_tick(g);
    }
    p2p_tick(g,now);
    ui_tick(g,now);
    if(g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE&&g->puzzle_wrong&&!g->history_browsing&&now-g->puzzle_feedback_at>=1100)puzzle_rewind(g,g->puzzle_step);
    if (g->scene == SCENE_HOSTJOIN) {
        hostjoin_tick(g);
        return;
    }
    if (g->scene == SCENE_ONLINE)return;
    if (g->scene != SCENE_GAME) return;

    if (g->mode == MODE_SINGLE) {
        single_tick(g, now);
    } else if (g->mode == MODE_LOCAL) {
        if(!net_is_p2p(g->net))local_tick(g);
    } else if (g->mode == MODE_ONLINE) {
        /* Updated by the global online pump above. */
    } else if (g->mode == MODE_ANALYSIS) {
        if (g->review_on) { review_step(g); return; }
        if (g->eval_ai) {
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
                if (tree_synced(g)) {
                    g->tree_cur->has_eval = true;
                    g->tree_cur->eval_cp = g->state == CHECKMATE ? -30000
                        : g->board.side == WHITE ? cp : -cp;
                    g->tree_cur->eval_mate = g->board.side == WHITE ? mate : -mate;
                    if(g->eval_has_mate&&g->state!=CHECKMATE)g->tree_cur->eval_cp=g->tree_cur->eval_mate>0?30000:-30000;
                    MoveNode *node=g->tree_cur,*parent=node->parent;
                    if(depth>=8&&node->cls==RC_NONE&&node->engine_choice_known&&parent&&parent->has_eval){
                        MoveList legal;gen_legal(&parent->board,&legal);
                        node->cls=review_classify(parent->eval_cp,parent->eval_mate!=0,-node->eval_cp,node->eval_mate!=0,
                            parent->eval_cp,parent->eval_mate!=0,false,g->book&&opening_is_book(g->book,&node->board),legal.count,node->engine_choice);
                        if(g->ply>0)g->review_cls[g->ply-1]=node->cls;
                    }
                }
                g->eval_valid = true;
            }
            g->eng_line_count = ai_get_lines(g->eval_ai, g->eng_lines, AI_MAX_LINES);
            if (g->eng_line_count > 0) {
                const char *pv = g->eng_lines[0].pv;
                int n = 0;
                while (pv[n] && pv[n] != ' ' && n < 7) n++;
                if (n >= 4) {
                    memcpy(g->eval_best_uci, pv, (size_t)n);
                    g->eval_best_uci[n] = '\0';
                }
            }
            /* A capped search ends on bestmove; keep the analysis cycling. */
            if (done && (g->eng_depth > 0 || g->eng_time_ms > 0))
                eval_restart(g);
        }
    }
}

/* ------------------------------------------------------------------ */
/* rendering                                                           */
/* ------------------------------------------------------------------ */

/* Compute the flying piece transform for a given animation style. */
static void anim_piece_transform(const Gui *g, AnimStyle style, float t,
                                 float fx, float fy, float tx, float ty,
                                 float *x, float *y, float *scale, float *alpha)
{
    (void)g;
    switch (style) {
        case ANIM_ARCADE: {
            /* Straight 2D slide (no z-hop, no magnify): ease-out cubic. The
             * comet trail is drawn separately by the caller. */
            float u = 1.0f - t;
            float e = 1.0f - u * u * u;
            *x = fx + (tx - fx) * e;
            *y = fy + (ty - fy) * e;
            *scale = 1.0f;
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
        *scale = 1.0f - 0.45f * t;
        *alpha = 1.0f - t;
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
    SDL_SetTextureAlphaMod(t, (Uint8)(alpha * 255.0f));
#if SDL_VERSION_ATLEAST(2, 0, 10)
    SDL_FRect rc = { cx - w * 0.5f * s, cy - h * 0.5f * s, w * s, h * s };
    SDL_RenderCopyF(ren, t, NULL, &rc);
#else
    SDL_Rect rc = { (int)(cx - w * 0.5f * s), (int)(cy - h * 0.5f * s),
                    (int)(w * s), (int)(h * s) };
    SDL_RenderCopy(ren, t, NULL, &rc);
#endif
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
        /* A captured piece is drawn by the animation (grows back in reverse). */
        if (s->cap_sq >= 0 && s->cap_sq == sq) return true;
        if (i > 0) {  /* pending steps still show their pre-move picture */
            if (s->from == sq) return true;
        }
    }
    return false;
}

/* Thick circle ring (annulus) drawn with filled circles. */
static void draw_ring_thick(SDL_Renderer *ren, int cx, int cy, int rad,
                            int thick, Uint8 r, Uint8 gg, Uint8 b, Uint8 a);
/* High-res anti-aliased red capture ring (with glow). */
static void draw_capture_ring(Gui *g, SDL_Renderer *ren, int cx, int cy,
                              int sq, Uint8 alpha);

/* Straight trail behind a sliding piece: grows to three squares and fades
 * on arrival. Cyan for White pieces, coral for Black. */
static void draw_move_trail(SDL_Renderer *ren, Piece piece,
                            float fx, float fy, float tx, float ty, float t,float square_size)
{
    if (!WHITE_PIECE(piece) && !BLACK_PIECE(piece)) return;
    bool white = WHITE_PIECE(piece);
    Uint8 cr = white ? 64 : 244, cg = white ? 183 : 91, cb = white ? 231 : 102;

    float ex = tx - fx, ey = ty - fy;
    float D = sqrtf(ex * ex + ey * ey);
    if (D < 2.0f) return;
    float ux = ex / D, uy = ey / D;

    /* Keep the tail inside the travelled segment, including at the start.
     * Let it fade behind the arriving piece, as in Trail.mov. */
    float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    float L = e * D;
    float maxL = 3.0f * square_size;
    if (L > maxL) L = maxL;
    if (L < 1.0f) return;

    /* Head is the piece's current centre; tail is L back along the direction. */
    float hx = fx + ex * e, hy = fy + ey * e;
    float tlx = hx - ux * L, tly = hy - uy * L;
    float hw = square_size * 0.30f;
    float px = -uy * hw, py = ux * hw;
    Uint8 opacity = (Uint8)(202.0f * (1.0f - t * t * t));

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_Vertex v[4];
    v[0].position = (SDL_FPoint){ hx + px, hy + py };
    v[1].position = (SDL_FPoint){ hx - px, hy - py };
    v[2].position = (SDL_FPoint){ tlx - px, tly - py };
    v[3].position = (SDL_FPoint){ tlx + px, tly + py };
    SDL_Color head = { cr, cg, cb, opacity };
    SDL_Color tail = { cr, cg, cb, 0 };
    v[0].color = head; v[1].color = head;
    v[2].color = tail; v[3].color = tail;
    for (int i = 0; i < 4; i++) {
        v[i].tex_coord = (SDL_FPoint){ 0, 0 };
    }
    int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(ren, NULL, v, 4, idx, 6);
#else
    int N = (int)(L / (square_size * 0.10f)) + 2;
    if (N > 64) N = 64;
    for (int i = 0; i <= N; i++) {
        float s = (float)i / (float)N;           /* 0 at head, 1 at tail */
        int cx = (int)lroundf(hx - ux * L * s);
        int cy = (int)lroundf(hy - uy * L * s);
        int a = (int)(opacity * (1.0f - s));
        SDL_SetRenderDrawColor(ren, cr, cg, cb, (Uint8)a);
        fill_circle(ren, cx, cy, (int)hw);
    }
#endif
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static float smoothstepf(float a, float b, float x)
{
    if (b <= a) return x >= b ? 1.0f : 0.0f;
    float t = (x - a) / (b - a);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/* 0 -> 1 -> 0 envelope (fade in by b, hold to c, fade out by d). */
static float env(float p, float a, float b, float c, float d)
{
    return smoothstepf(a, b, p) * (1.0f - smoothstepf(c, d, p));
}

/* Check animation (per check.mov): a near-opaque light mask over the king's
 * square, an inner rectangle that expands from the centre, and four corner
 * brackets that grow from framing the king to the edges, then everything fades.
 * `p` runs 0..1 over the animation. */
static void draw_check_overlay(Gui *g, SDL_Renderer *ren, int sq, float p)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    int cx = rc.x + rc.w / 2, cy = rc.y + rc.h / 2;
    int S = g->sq;

    SDL_RenderSetClipRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    /* near-opaque light mask */
    int ma = (int)(120.0f * env(p, 0.0f, 0.35f, 0.55f, 0.92f));
    if (ma > 0) {
        SDL_SetRenderDrawColor(ren, 235, 238, 242, (Uint8)ma);
        SDL_RenderFillRect(ren, &rc);
    }

    /* inner rectangle, expanding from the centre (its flat edges are the
     * horizontal bars seen mid-animation) */
    int ra = (int)(255.0f * env(p, 0.04f, 0.20f, 0.60f, 0.86f));
    if (ra > 0) {
        float s = 0.06f + 0.90f * smoothstepf(0.08f, 0.68f, p);
        int half = (int)(S * 0.5f * s);
        int th = S * 3 / 100;
        if (th < 2) th = 2;
        SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)ra);
        SDL_Rect top = { cx - half, cy - half, 2 * half, th };
        SDL_Rect bot = { cx - half, cy + half - th, 2 * half, th };
        SDL_Rect lef = { cx - half, cy - half, th, 2 * half };
        SDL_Rect rig = { cx + half - th, cy - half, th, 2 * half };
        SDL_RenderFillRect(ren, &top);
        SDL_RenderFillRect(ren, &bot);
        SDL_RenderFillRect(ren, &lef);
        SDL_RenderFillRect(ren, &rig);
    }

    /* corner brackets, growing from the king to the square edges */
    int ba = (int)(255.0f * env(p, 0.0f, 0.16f, 0.72f, 0.96f));
    if (ba > 0) {
        float s = 0.46f + 0.92f * smoothstepf(0.05f, 0.70f, p);
        int half = (int)(S * 0.5f * s);
        int len = S * 30 / 100;
        int th = S * 45 / 1000;
        if (th < 3) th = 3;
        int L = cx - half, R = cx + half, T = cy - half, B = cy + half;
        SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)ba);
        for (int i = 0; i < th; i++) {
            SDL_RenderDrawLine(ren, L, T + i, L + len, T + i);
            SDL_RenderDrawLine(ren, L + i, T, L + i, T + len);
            SDL_RenderDrawLine(ren, R, T + i, R - len, T + i);
            SDL_RenderDrawLine(ren, R - i, T, R - i, T + len);
            SDL_RenderDrawLine(ren, L, B - i, L + len, B - i);
            SDL_RenderDrawLine(ren, L + i, B, L + i, B - len);
            SDL_RenderDrawLine(ren, R, B - i, R - len, B - i);
            SDL_RenderDrawLine(ren, R - i, B, R - i, B - len);
        }
    }

    SDL_RenderSetClipRect(ren, NULL);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static float click_release_progress(const Gui *g, Uint32 now)
{
    if (!g->pick_released) return 0.0f;
    Sint32 elapsed = (Sint32)(now - g->pick_released);
    return smoothstepf(0, 180, elapsed > 0 ? (float)elapsed : 0.0f);
}

static float click_pick_strength(const Gui *g, Uint32 now)
{
    if (!g->pick_started || g->anim_style == ANIM_NONE) return 0.0f;
    if (g->pick_released) {
        return 1.0f - click_release_progress(g, now);
    }
    Sint32 elapsed = (Sint32)(now - g->pick_started);
    return smoothstepf(0, 16, elapsed > 0 ? (float)elapsed : 0.0f);
}

static float pick_scale(const Gui *g, Uint32 now, bool dragging)
{
    if (!g->pick_started || g->anim_style == ANIM_NONE) return 1.0f;
    Sint32 elapsed = (Sint32)(now - g->pick_started);
    if (elapsed < 0) elapsed = 0;
    if (dragging) {
        /* Repeat for as long as the pointer holds the piece, including when
         * the pointer stops moving. Phase belongs to this drag, not the app. */
        float phase = (float)(elapsed % 900) / 900.0f;
        float pulse = 0.5f - 0.5f * cosf(phase * 6.28318531f);
        return 1.0f + 0.16f * smoothstepf(0, 120, (float)elapsed) * pulse;
    }
    return 1.0f + 0.04f * click_pick_strength(g, now);
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
            if (sq == g->selected && g->pick_started && g->anim_style != ANIM_NONE) {
                /* A click anywhere in the square picks up at the pointer.
                 * Release interpolates that exact point back to the centre. */
                float back = click_release_progress(g, now);
                SDL_Point cursor = g->pick_released ? g->pick_position : g->mouse;
                cx = cursor.x + (cx - cursor.x) * back;
                cy = cursor.y + (cy - cursor.y) * back;
            }
            float lift = sq == g->selected ? pick_scale(g, now, false) : 1.0f;
            render_piece_box(g, ren, p, cx, cy, g->sq * lift, 1.0f);
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

        /* captured piece: red ring + shrink/fade (or grow back on reverse) */
        if (s->cap_sq >= 0 && s->captured != EMPTY) {
            float cx, cy, sc, al;
            piece_center(g, s->cap_sq, &cx, &cy);
            if (s->reverse) { sc = t; al = t; }
            else            captured_transform(g->anim_style, t, &sc, &al);
            if (g->anim_style == ANIM_ARCADE && !s->reverse) {
                Uint8 ra = (Uint8)(210.0f * (1.0f - t));
                draw_capture_ring(g, ren, (int)cx, (int)cy, g->sq, ra);
            }
            render_piece_box(g, ren, s->captured, cx, cy, g->sq * sc, al);
        }

        float fx, fy, tx, ty, x, y, sc, al;
        piece_center(g, s->from, &fx, &fy);
        piece_center(g, s->to, &tx, &ty);
        if (s->fast) { fx = s->cursor_x; fy = s->cursor_y; }

        /* Comet trail behind the sliding piece. */
        if (g->anim_style == ANIM_ARCADE && !s->fast)
            draw_move_trail(ren,s->piece,fx,fy,tx,ty,t,g->sq);

        anim_piece_transform(g, g->anim_style, t, fx, fy, tx, ty, &x, &y, &sc, &al);
        if (s->fast) sc *= 1.0f + (s->cursor_scale - 1.0f) * (1.0f - t);
        render_piece_box(g, ren, s->piece, x, y, g->sq * sc, al);

        if (s->has_rook) {
            piece_center(g, s->rfrom, &fx, &fy);
            piece_center(g, s->rto, &tx, &ty);
            anim_piece_transform(g, g->anim_style, t, fx, fy, tx, ty, &x, &y, &sc, &al);
            render_piece_box(g, ren, s->rook, x, y, g->sq * sc, al);
        }
    }
}

/* End effects from Checkmate.mov / Draw.mov: wash the king squares, show
 * labelled vector symbols, then shrink them into permanent corner badges. */
static void draw_end_overlay(Gui *g, SDL_Renderer *ren, Uint32 now)
{
    bool draw = position_is_draw(g);
    if ((!draw && g->state != CHECKMATE) || g->anim_count) return;
    if (g->end_anim && (Sint32)(now - g->end_anim) < 0) return;
    float p = g->end_anim ? (float)(now - g->end_anim) / 1400.0f : 1.0f;
    float settle = smoothstepf(0.50f, 1.0f, p);
    float show = smoothstepf(0.0f, 0.12f, p);
    for (int side = WHITE; side <= BLACK; side++) {
        int sq = find_king(&g->board, (Color)side);
        if (sq < 0) continue;
        bool winner = !draw && side != (int)g->board.side;
        int icon = draw ? 2 : winner ? 0 : 1;
        SDL_Color c = draw ? (SDL_Color){112, 110, 106, 255}
                          : winner ? (SDL_Color){130, 182, 77, 255}
                                   : (SDL_Color){210, 39, 48, 255};
        SDL_Rect rc;
        window_sq(g, sq, &rc);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b,
                              (Uint8)(185 * show * (1.0f - settle)));
        SDL_RenderFillRect(ren, &rc);
        float cx = rc.x + rc.w * 0.5f + rc.w * 0.43f * settle;
        float cy = rc.y + rc.h * 0.5f - rc.h * 0.43f * settle;
        float rad = g->sq * (0.38f - 0.19f * settle);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, (Uint8)(45 * settle));
        fill_circle(ren, (int)cx, (int)cy + 1, (int)rad + 1);
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, (Uint8)(255 * settle));
        fill_circle(ren, (int)cx, (int)cy, (int)rad);
        if (g->end_tex[icon]) {
            int size = (int)(rad * 2.1f);
            SDL_Rect dst = { (int)cx - size / 2, (int)cy - size / 2, size, size };
            SDL_SetTextureAlphaMod(g->end_tex[icon], (Uint8)(255 * show));
            SDL_RenderCopy(ren, g->end_tex[icon], NULL, &dst);
            SDL_SetTextureAlphaMod(g->end_tex[icon], 255);
        }
        float label_alpha = show * (1.0f - smoothstepf(0.45f, 0.75f, p));
        if (label_alpha > 0.01f) {
            const char *label = draw ? "Draw" : winner ? "Winner" : "Checkmate";
            int tw = 0, th = 0;
            TTF_SizeUTF8(g->font_move, label, &tw, &th);
            int w = (int)lroundf(tw / eff_scale(g)) + 16;
            int h = text_height(g, g->font_move) + 8;
            int x = clampi(rc.x + rc.w / 2 - w / 2, g->board_x,
                           g->board_x + 8 * g->sq - w);
            int y = rc.y - h / 2;
            if (y < g->board_y + 2) y = g->board_y + 2;
            SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)(255 * label_alpha));
            SDL_Rect pill = { x + h / 2, y, w - h, h };
            SDL_RenderFillRect(ren, &pill);
            fill_circle(ren, x + h / 2, y + h / 2, h / 2);
            fill_circle(ren, x + w - h / 2, y + h / 2, h / 2);
            c.a = (Uint8)(255 * label_alpha);
            render_text_centered(g, g->font_move, label, x + w / 2, y + 4, c);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
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

/* Thick circle ring (annulus) drawn with filled circles. */
static void draw_ring_thick(SDL_Renderer *ren, int cx, int cy, int rad,
                            int thick, Uint8 r, Uint8 gg, Uint8 b, Uint8 a)
{
    int outer = rad + thick / 2, inner = rad - thick / 2;
    if (inner < 0) inner = 0;
    if (thick < 2) thick = 2;
    float mid = (inner + outer) / 2.0f;
    int cr = thick / 2 + 1;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    /* Subtle outer glow: two slightly larger, low-alpha rings. */
    for (int k = 2; k >= 1; k--) {
        SDL_SetRenderDrawColor(ren, r, gg, b, (Uint8)(a / (k * 7 + 1)));
        float m2 = mid + (float)k * thick * 0.65f;
        int n2 = (int)(2.0f * GUI_PI * m2 / (cr * 0.45f)) + 8;
        if (n2 > 256) n2 = 256;
        for (int i = 0; i < n2; i++) {
            float th = (float)i / (float)n2 * 2.0f * GUI_PI;
            fill_circle(ren, cx + (int)lroundf(cosf(th) * m2),
                        cy + (int)lroundf(sinf(th) * m2), cr);
        }
    }

    /* Crisp annulus (centres on the ring so there is no centre fill). */
    SDL_SetRenderDrawColor(ren, r, gg, b, a);
    int n = (int)(2.0f * GUI_PI * mid / (cr * 0.45f)) + 8;
    if (n > 256) n = 256;
    for (int i = 0; i < n; i++) {
        float th = (float)i / (float)n * 2.0f * GUI_PI;
        fill_circle(ren, cx + (int)lroundf(cosf(th) * mid),
                    cy + (int)lroundf(sinf(th) * mid), cr);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* Draw the high-res capture ring scaled to the square (texture ring centreline
 * is at 0.36 of the texture size). Falls back to the raster ring. */
static void draw_capture_ring(Gui *g, SDL_Renderer *ren, int cx, int cy,
                              int sq, Uint8 alpha)
{
    if (!g->capture_ring) {
        draw_ring_thick(ren, cx, cy, (int)(sq * 0.42f), (int)(sq * 0.08f),
                        225, 80, 70, alpha);
        return;
    }
    float size = (sq * 0.42f) / 0.36f;
    SDL_Rect dst = { (int)lroundf(cx - size / 2.0f),
                     (int)lroundf(cy - size / 2.0f),
                     (int)lroundf(size), (int)lroundf(size) };
    SDL_SetTextureAlphaMod(g->capture_ring, alpha);
    SDL_RenderCopy(ren, g->capture_ring, NULL, &dst);
    SDL_SetTextureAlphaMod(g->capture_ring, 255);
}

/* chess.com-style destination indicator: a soft dot for a quiet move, a thick
 * red ring for a capture. */
static void draw_target_dot(Gui *g, SDL_Renderer *ren, int sq, bool capture)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    int cx = rc.x + rc.w / 2, cy = rc.y + rc.h / 2;
    if (capture) {
        draw_capture_ring(g, ren, cx, cy, g->sq, 225);
    } else {
        int rad = (int)(g->sq * 0.15f);
        if(g->target_dot){
            int size=(int)lroundf(g->sq*0.50f);
            SDL_Rect target={cx-size/2,cy-size/2,size,size};
            SDL_RenderCopy(ren,g->target_dot,NULL,&target);return;
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,28,43,61,18);fill_circle(ren,cx,cy+1,rad+2);
        SDL_SetRenderDrawColor(ren,28,43,61,24);fill_circle(ren,cx,cy+1,rad+1);
        SDL_SetRenderDrawColor(ren, 247, 249, 252, 210);
        fill_circle(ren, cx, cy, rad);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
}

/* Soft radial glow behind the selected piece. */
static void draw_selection_glow(Gui *g, SDL_Renderer *ren, int sq, Uint32 now)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    float strength = g->dragging ? 1.0f : click_pick_strength(g, now);
    if (!g->dragging && g->pick_glow && strength > 0.0f) {
        float progress = click_release_progress(g, now);
        Uint32 growth_at = g->pick_released ? g->pick_released : now;
        Sint32 elapsed = (Sint32)(growth_at - g->pick_started);
        float held_radius = 0.35f + 0.30f * smoothstepf(0, 80, elapsed > 0 ? elapsed : 0);
        float size = g->sq * 2.0f * (held_radius + 0.25f * progress);
        SDL_Point cursor = g->pick_released ? g->pick_position : g->mouse;
        int frame = (int)(progress * (PICK_GLOW_FRAMES - 1));
        SDL_Rect src = { (frame % PICK_GLOW_COLS) * PICK_GLOW_SIZE,
                         (frame / PICK_GLOW_COLS) * PICK_GLOW_SIZE,
                         PICK_GLOW_SIZE, PICK_GLOW_SIZE };
        SDL_Rect dst = { (int)lroundf(cursor.x - size / 2), (int)lroundf(cursor.y - size / 2),
                         (int)lroundf(size), (int)lroundf(size) };
        SDL_SetTextureAlphaMod(g->pick_glow, (Uint8)(150 * strength));
        SDL_RenderCopy(ren, g->pick_glow, &src, &dst);
        SDL_SetTextureAlphaMod(g->pick_glow, 255);
    }
    SDL_SetRenderDrawColor(ren, 80, 220, 245, g->dragging ? 26 : 60);
    SDL_RenderFillRect(ren, &rc);
    /* The frame disappears on release; only the disc continues contracting. */
    float frame = g->dragging || !g->pick_released ? strength : 0.0f;
    SDL_SetRenderDrawColor(ren, 95, 232, 255, (Uint8)(240 * frame));
    int thick = g->sq * 4 / 100;
    if (thick < 2) thick = 2;
    for (int i = 0; i < thick; i++) {
        SDL_Rect border = { rc.x + i, rc.y + i, rc.w - 2 * i, rc.h - 2 * i };
        SDL_RenderDrawRect(ren, &border);
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
    Uint32 now=SDL_GetTicks();float dt=(now-g->promo_hover_at)/160.0f;g->promo_hover_at=now;if(dt>1)dt=1;
    Piece choices[4]={WQ,WN,WR,WB};
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    for(int i=0;i<4;i++){
        float target=pt_in(&pr[i],g->mouse.x,g->mouse.y)?1.0f:0.0f;
        float t=g->promo_hover[i];t+=target>t?dt:-dt;t=fmaxf(0,fminf(1,t));g->promo_hover[i]=t;
        float smooth=t*t*(3-2*t);
        ui_round(ren,pr[i],(SDL_Color){235,235,235,140},pr[i].w/2);
        ui_round(ren,pr[i],(SDL_Color){216,87,53,(Uint8)(240*smooth)},(int)((1-smooth)*pr[i].w/2));
        Piece pc=color_of_piece(g->board.board[g->promo_from])?choices[i]:(Piece)(choices[i]+BP-WP);
        render_piece_box(g,ren,pc,pr[i].x+pr[i].w/2.0f,pr[i].y+pr[i].h/2.0f,pr[i].h*(0.78f+0.20f*smooth),1.0f);
    }
}

static void render_status(Gui *g)
{
    char line[64];
    if(g->mode==MODE_LOCAL&&g->p2p_started&&g->override_result[0])
        snprintf(line,sizeof line,"%s",!strcmp(g->override_result,"*")?ui_tr(g,"Connection closed · game stopped","连接已断开 · 对局已停止"):
                 !strcmp(g->override_result,"1/2-1/2")?ui_tr(g,"Game over · Draw","对局结束 · 和棋"):
                 !strcmp(g->override_result,"1-0")?ui_tr(g,"Game over · White wins","对局结束 · 白方获胜"):ui_tr(g,"Game over · Black wins","对局结束 · 黑方获胜"));
    else if (g->state == CHECKMATE)
        snprintf(line,sizeof line,ui_tr(g,"Checkmate - %s wins","将杀 — %s获胜"),
                 ui_old_text(g,g->board.side==WHITE?"Black":"White"));
    else if (g->state == STALEMATE)
        snprintf(line, sizeof line, "Stalemate");
    else if (g->state == INSUFFICIENT_MATERIAL)
        snprintf(line, sizeof line, "Draw (insufficient material)");
    else if (g->state == THREEFOLD_REPETITION)
        snprintf(line, sizeof line, "Draw (threefold repetition)");
    else if (g->state == FIFTY_MOVE_RULE)
        snprintf(line, sizeof line, "Draw (fifty-move rule)");
    else {
        snprintf(line,sizeof line,ui_tr(g,"%s to move%s","%s行棋%s"),ui_old_text(g,g->board.side==WHITE?"White":"Black"),
                 in_check(&g->board,g->board.side)?ui_tr(g," - CHECK"," — 将军"):"");
    }
    render_text(g, g->font_ui, line, g->panel_x, g->board_y, (SDL_Color){ 30, 54, 78, 255 });

    if (g->mode == MODE_SINGLE) {
        char info[96];
        snprintf(info, sizeof info, "You: %s    AI: %s%s",
                 g->human_color == WHITE ? "White" : "Black",
                 AI_LEVELS[g->setup_level].label,
                 g->ai_thinking ? "   (thinking...)" : "");
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 65, 102, 138, 255 });
    } else if (g->mode == MODE_LOCAL) {
        bool ok = g->net && transport_state(g->net) == NET_STATE_CONNECTED;
        char info[200];
        if(g->p2p_started){
            int w=g->p2p_remaining[0],b=g->p2p_remaining[1];
            if(g->p2p_base)snprintf(info,sizeof info,ui_tr(g,"P2P · %s · %d:%02d / %d:%02d","P2P · 你执%s · %d:%02d／%d:%02d"),ui_old_text(g,g->local_color==WHITE?"White":"Black"),w/60000,w/1000%60,b/60000,b/1000%60);
            else snprintf(info,sizeof info,ui_tr(g,"P2P · You: %s · Unlimited","P2P · 你执%s · 不限时"),ui_old_text(g,g->local_color==WHITE?"White":"Black"));

        }else snprintf(info,sizeof info,ui_tr(g,"You: %s · %s","你执%s · %s"),ui_old_text(g,g->local_color==WHITE?"White":"Black"),ok?ui_tr(g,"connected","已连接"):ui_tr(g,"disconnected","已断开"));
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 65, 102, 138, 255 });
        if(g->p2p_started){char score[100];snprintf(score,sizeof score,ui_tr(g,"W/D/L %d/%d/%d%s","胜／和／负 %d／%d／%d%s"),g->p2p_score[0],g->p2p_score[1],g->p2p_score[2],g->draw_offered?ui_tr(g," · Draw offered"," · 收到和棋提议"):"");render_text(g,g->font_small,score,g->panel_x,g->board_y+49,(SDL_Color){65,102,138,255});}
    } else if (g->mode == MODE_ONLINE) {
        char info[160];
        const char *opp = g->online ? online_opponent(g->online) : "Opponent";
        bool spec = g->online && online_spectating(g->online);
        const char *role = spec ? "Spectating"
                                : (g->human_color == WHITE ? "You: White" : "You: Black");
        if (g->online && online_has_clocks(g->online)) {
            int w, b;
            online_clocks(g->online, &w, &b);
            snprintf(info, sizeof info, "%s  vs %s   W %d:%02d  B %d:%02d",
                     role, spec ? online_room_code(g->online) : opp,
                     w / 60000, (w / 1000) % 60, b / 60000, (b / 1000) % 60);
        } else {
            snprintf(info, sizeof info, "%s  vs %s",
                     role, spec ? online_room_code(g->online) : opp);
        }
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 65, 102, 138, 255 });
        if (g->draw_offered)
            render_text(g, g->font_small, "Draw offered",
                        g->panel_x, g->board_y + 50, (SDL_Color){ 240, 200, 120, 255 });
    } else if (g->mode == MODE_PUZZLE) {
        char info[160];
        const char *st = g->puzzle_done ? "Solved!"
                       : g->puzzle_failed ? "Answer shown"
                                          : (g->board.side == g->human_color
                                                 ? "Your move" : "...");
        snprintf(info, sizeof info, "%s", ui_old_text(g,st));
        render_text(g, g->font_small, info, g->panel_x, g->board_y + 30,
                    (SDL_Color){ 65, 102, 138, 255 });
    } else if (g->mode == MODE_ANALYSIS) {
        if (!g->eval_ai)
            render_text(g, g->font_small, "Analysis: no engine",
                        g->panel_x, g->board_y + (g->ply > 0 ? 52 : 30),
                        (SDL_Color){ 65, 102, 138, 255 });
    }
}

/* Filled rectangle with independently-rounded top/bottom corners. */
static void fill_round_rect(SDL_Renderer *ren, SDL_Rect r, int rad,
                            bool round_top, bool round_bottom)
{
    if (rad <= 0) { SDL_RenderFillRect(ren, &r); return; }
    int t = round_top ? rad : 0;
    int b = round_bottom ? rad : 0;
    SDL_Rect body = { r.x, r.y + t, r.w, r.h - t - b };
    SDL_RenderFillRect(ren, &body);
    if (!round_top) {
        SDL_Rect cap = { r.x, r.y, r.w, rad };
        SDL_RenderFillRect(ren, &cap);
    } else {
        SDL_Rect cap = { r.x + rad, r.y, r.w - 2 * rad, rad };
        SDL_RenderFillRect(ren, &cap);
        for (int dy = 0; dy < rad; dy++) {
            int dx = (int)sqrtf((float)(rad * rad - (rad - 1 - dy) * (rad - 1 - dy)));
            SDL_RenderDrawLine(ren, r.x + rad - dx, r.y + dy, r.x + rad, r.y + dy);
            SDL_RenderDrawLine(ren, r.x + r.w - rad, r.y + dy,
                               r.x + r.w - rad + dx, r.y + dy);
        }
    }
    if (!round_bottom) {
        SDL_Rect cap = { r.x, r.y + r.h - rad, r.w, rad };
        SDL_RenderFillRect(ren, &cap);
    } else {
        SDL_Rect cap = { r.x + rad, r.y + r.h - rad, r.w - 2 * rad, rad };
        SDL_RenderFillRect(ren, &cap);
        for (int dy = 0; dy < rad; dy++) {
            int dx = (int)sqrtf((float)(rad * rad - (rad - 1 - dy) * (rad - 1 - dy)));
            int yy = r.y + r.h - 1 - dy;
            SDL_RenderDrawLine(ren, r.x + rad - dx, yy, r.x + rad, yy);
            SDL_RenderDrawLine(ren, r.x + r.w - rad, yy, r.x + r.w - rad + dx, yy);
        }
    }
}

/* Wide vertical evaluation bar to the left of the board (analysis mode). The
 * white portion follows the side at the bottom of the board, and the numeric
 * score (always from White's point of view) is drawn inside near the top. */
static void draw_eval_bar(Gui *g, SDL_Renderer *ren)
{
    if (g->mode != MODE_ANALYSIS || !g->eval_valid) return;

    int w = EVAL_BAR_W;
    int x = g->board_x - w - 8;
    int y = g->board_y, h = 8 * g->sq;
    if (x < 2) x = 2;
    int rad = 8;

    float f;   /* White's share, 0..1 */
    if (g->eval_has_mate) {
        if (g->eval_mate > 0)      f = 1.0f;   /* White mates */
        else if (g->eval_mate < 0) f = 0.0f;   /* Black mates */
        else f = (g->eval_side == WHITE) ? 0.0f : 1.0f;  /* side to move is mated */
    } else {
        f = 1.0f / (1.0f + expf(-(float)g->eval_cp / 400.0f));
    }

    SDL_Rect bar = { x, y, w, h };
    set_render_color(ren, 38, 40, 46);
    fill_round_rect(ren, bar, rad, true, true);

    /* White grows from the bottom when White is at the bottom, else from top. */
    int wh = (int)(h * f);
    if (wh < 2) wh = 2;
    if (wh > h) wh = h;
    set_render_color(ren, 236, 236, 238);
    if (!g->flipped) {
        SDL_Rect white = { x, y + h - wh, w, wh };
        fill_round_rect(ren, white, rad, false, true);
    } else {
        SDL_Rect white = { x, y, w, wh };
        fill_round_rect(ren, white, rad, true, false);
    }

    /* midpoint marker */
    set_render_color(ren, 200, 120, 40);
    SDL_RenderDrawLine(ren, x + 2, y + h / 2, x + w - 2, y + h / 2);

    SDL_Rect border = { x, y, w, h };
    set_render_color(ren, 120, 122, 130);
    SDL_RenderDrawRect(ren, &border);

    /* numeric score inside near the top (White's perspective) */
    char val[24];
    if (g->eval_has_mate) {
        if (g->eval_mate == 0) snprintf(val, sizeof val, "#");
        else snprintf(val, sizeof val, "%sM%d", g->eval_mate > 0 ? "+" : "-",
                      g->eval_mate > 0 ? g->eval_mate : -g->eval_mate);
    } else {
        snprintf(val, sizeof val, "%+.1f", g->eval_cp / 100.0);
    }

    /* Keep a fixed, high-contrast score label independent of app theme. */
    int tw=0,th=0;TTF_SizeUTF8(g->font_tiny,val,&tw,&th);float scale=eff_scale(g);
    SDL_Rect label={x+1,y+2,w-2,text_height(g,g->font_tiny)+8};
    SDL_SetRenderDrawColor(ren,236,236,238,255);SDL_RenderFillRect(ren,&label);
    render_text_exact(g,g->font_tiny,val,x+(w-(int)(tw/scale))/2,y+6,(SDL_Color){30,32,38,255});
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

/* SAN of the first move of a UCI principal variation. */
static void pv_first_san(const Gui *g, const char *pv, char *out, size_t n)
{
    out[0] = '\0';
    while (*pv == ' ') pv++;
    char tok[8];
    int i = 0;
    while (*pv && *pv != ' ' && i < 7) tok[i++] = *pv++;
    tok[i] = '\0';
    if (i < 4) return;
    Move m;
    if (!ai_uci_to_move(&g->board, tok, &m)) return;
    move_to_san(&g->board, m, out, n);
}

/* ---- move review (analysis) ---- */

static int review_material(const Board *b, Color c)
{
    int s = 0;
    for (int i = 0; i < 64; i++) {
        Piece p = b->board[i];
        bool white = WHITE_PIECE(p);
        if ((c == WHITE) != white || p == EMPTY) continue;
        switch (p) {
            case WP: case BP: s += 100; break;
            case WN: case BN: s += 320; break;
            case WB: case BB: s += 330; break;
            case WR: case BR: s += 500; break;
            case WQ: case BQ: s += 900; break;
            default: break;
        }
    }
    return s;
}

static void review_finish_ply(Gui *g, MoveNode *node)
{
    const Board *before = &node->parent->board;
    Move mv = node->move;
    Board after = *before;
    make_move_plumb(&after, mv);

    Color mover = before->side;
    /* A genuine sacrifice: the piece on its destination can be won by the
     * opponent (a cheaper attacker, or net material loss). */
    int to = MOVE_TO(mv);
    Color opp = (mover == WHITE) ? BLACK : WHITE;
    int loss = review_material(before, mover) - review_material(&after, mover);
    Piece moved = before->board[MOVE_FROM(mv)];
    bool sac = square_attacked(&after, to, opp) &&
               (loss > 0 || min_attacker_value(&after, to, opp) < piece_value(moved));
    MoveList ml;
    gen_legal(before, &ml);
    bool book = g->book && opening_is_book(g->book, &after);

    ReviewClass rc = review_classify(
        g->rb_cp, g->rb_hm, g->ra_cp, g->ra_hm,
        g->r2_cp, g->r2_hm, sac, book, ml.count,g->r_played_best);
    g->review_cls[g->review_i] = rc;
    node->cls = rc;

    /* Accuracy and centipawn loss for this move (mover's perspective). */
    double wb = g->rb_hm ? (g->rb_cp > 0 ? 100.0 : 0.0) : review_win_pct(g->rb_cp);
    double wa = g->ra_hm ? (g->ra_cp > 0 ? 100.0 : 0.0) : review_win_pct(g->ra_cp);
    g->review_acc[g->review_i] = review_move_accuracy(wb, wa);
    int cpl;
    if (g->rb_hm && !g->ra_hm) cpl = 1000;          /* gave up a forced mate */
    else if (g->ra_hm && g->rb_hm) cpl = 0;
    else cpl = g->rb_cp - g->ra_cp;
    if (cpl < 0) cpl = 0;
    g->review_cpl[g->review_i] = cpl;

    node->parent->has_eval = true;
    node->parent->eval_cp = g->rb_cp;
    node->parent->eval_mate = g->rb_mate;
    node->has_eval = true;
    node->eval_cp = -g->ra_cp;
    node->eval_mate = -g->ra_mate;
    if (g->ra_hm && g->ra_mate == 0 && game_state(&node->board) == CHECKMATE)
        node->eval_cp = -30000;

    g->review_i++;
    g->review_stage = 0;
}

static void review_finish(Gui *g)
{
    g->review_on = false;
    memset(g->rev_count, 0, sizeof g->rev_count);
    memset(g->rev_count_side, 0, sizeof g->rev_count_side);
    memset(g->rev_accuracy_side, 0, sizeof g->rev_accuracy_side);
    memset(g->rev_acpl_side, 0, sizeof g->rev_acpl_side);
    memset(g->rev_moves_side, 0, sizeof g->rev_moves_side);

    double acc_sum = 0.0, acc_side[2] = { 0.0, 0.0 };
    long cpl_sum = 0, cpl_side[2] = { 0, 0 };
    int counted = 0;

    for (int i = 0; i < g->review_n && i < MAX_PLY; i++) {
        MoveNode *node = g->review_nodes[i];
        if (!node || !node->parent) continue;
        int c = g->review_cls[i];
        int s = (node->parent->board.side == WHITE) ? 0 : 1;   /* the mover */
        if (c > RC_NONE && c < 12) {
            g->rev_count[c]++;
            g->rev_count_side[s][c]++;
        }
        if (c != RC_NONE && c != RC_FORCED && c != RC_BOOK) {
            acc_sum += g->review_acc[i];
            cpl_sum += g->review_cpl[i];
            acc_side[s] += g->review_acc[i];
            cpl_side[s] += g->review_cpl[i];
            g->rev_moves_side[s]++;
            counted++;
        }
    }
    g->rev_moves = counted;
    g->rev_accuracy = counted ? acc_sum / counted : 0.0;
    g->rev_acpl = counted ? (int)((cpl_sum + counted / 2) / counted) : 0;
    for (int s = 0; s < 2; s++) {
        int m = g->rev_moves_side[s];
        g->rev_accuracy_side[s] = m ? acc_side[s] / m : 0.0;
        g->rev_acpl_side[s] = m ? (int)((cpl_side[s] + m / 2) / m) : 0;
    }
    g->rev_report = true;

    char s[200];
    snprintf(s, sizeof s, "Review complete: accuracy %.1f%%, ACPL %d",
             g->rev_accuracy, g->rev_acpl);
    set_msg(g, s, NULL);
    analysis_refresh(g);
}

static void review_start(Gui *g)
{
    if (g->mode != MODE_ANALYSIS || g->ply < 1) return;
    if (!g->engine_path[0]) { set_msg(g, "Review needs an engine (Stockfish)", NULL); return; }
    if (!g->review_ai) g->review_ai = ai_start(g->engine_path);
    if (!g->review_ai) { set_msg(g, "Could not start review engine", NULL); return; }

    eval_stop(g);
    ai_set_multipv(g->review_ai, 2);
    ai_set_depth(g->review_ai, 0);
    ai_set_movetime(g->review_ai, 200);
    for (int i = 0; i < MAX_PLY; i++) g->review_cls[i] = RC_NONE;

    /* Snapshot the target line so navigating does not interrupt the review. */
    g->review_n = 0;
    for (int i = 0; i < g->path_len && i < MAX_PLY; i++) {
        MoveNode *n = g->path_nodes[i];
        if (n && n->parent) g->review_nodes[g->review_n++] = n;
    }
    if (g->review_n == 0) { set_msg(g, "Nothing to review", NULL); return; }

    g->rev_report = false;
    g->review_on = true;
    g->review_i = 0;
    g->review_stage = 0;
    g->r_issued = false;
    set_msg(g, "Reviewing ...", NULL);
}

static void review_step(Gui *g)
{
    if (!g->review_on) return;
    if (g->review_i >= g->review_n) { review_finish(g); return; }

    MoveNode *node = g->review_nodes[g->review_i];
    if (!node || !node->parent) { g->review_i++; g->review_stage = 0; return; }
    Board before = node->parent->board;
    Board after = before;
    Move played = node->move;
    make_move_plumb(&after, played);

    if (!g->r_issued) {
        ai_go(g->review_ai, g->review_stage == 0 ? &before : &after);
        g->r_issued = true;
    }

    char uci[8];
    if (!ai_poll_bestmove(g->review_ai, uci)) return;   /* still searching */
    g->r_issued = false;

    if (g->review_stage == 0) {
        AiLine lines[2];
        int n = ai_get_lines(g->review_ai, lines, 2);
        int cp, mate, depth;
        ai_get_eval(g->review_ai, &cp, &mate, &depth);
        if (n > 0) {
            g->rb_cp = lines[0].cp; g->rb_mate = lines[0].mate;
            g->rb_hm = lines[0].has_mate;
            if(g->rb_hm)g->rb_cp=g->rb_mate>0?30000:-30000;
        } else {
            g->rb_cp = cp; g->rb_mate = mate;
            g->rb_hm = ai_eval_has_mate(g->review_ai);
            if(g->rb_hm)g->rb_cp=g->rb_mate>0?30000:-30000;
        }
        if (n > 1) {
            g->r2_cp = lines[1].cp; g->r2_mate = lines[1].mate;
            g->r2_hm = lines[1].has_mate;
            if(g->r2_hm)g->r2_cp=g->r2_mate>0?30000:-30000;
        } else {
            g->r2_cp = g->rb_cp; g->r2_mate = g->rb_mate; g->r2_hm = g->rb_hm;
        }

        Move bestm;
        bool played_best = false;
        if (ai_uci_to_move(&before,uci,&bestm))
            played_best = (MOVE_FROM(bestm) == MOVE_FROM(played) &&
                           MOVE_TO(bestm) == MOVE_TO(played) &&
                           (!(MOVE_FLAGS(played)&FLAG_PROMO)||MOVE_PROMO(bestm)==MOVE_PROMO(played)));
        g->r_played_best=played_best;

        if (played_best) {
            g->ra_cp = g->rb_cp; g->ra_mate = g->rb_mate; g->ra_hm = g->rb_hm;
            review_finish_ply(g, node);
        } else {
            g->review_stage = 1;
        }
        return;
    }

    int cp, mate, depth;
    ai_get_eval(g->review_ai, &cp, &mate, &depth);
    g->ra_cp = -cp; g->ra_mate = -mate;
    g->ra_hm = ai_eval_has_mate(g->review_ai);
    if(g->ra_hm)g->ra_cp=mate>0?-30000:30000;
    review_finish_ply(g, node);
}

static void render_review_bar(Gui *g, SDL_Renderer *ren)
{
    if (!g->review_on) return;
    int x = g->panel_x, y = g->board_y + 150, w = 380, h = 12;
    set_render_color(ren, 45, 45, 50);
    SDL_Rect bg = { x, y, w, h };
    SDL_RenderFillRect(ren, &bg);
    float f = g->review_n ? (float)g->review_i / (float)g->review_n : 0.0f;
    SDL_Rect fg = { x, y, (int)(w * f), h };
    set_render_color(ren, 90, 150, 200);
    SDL_RenderFillRect(ren, &fg);
    char t[64];
    snprintf(t, sizeof t, "Reviewing %d / %d", g->review_i, g->review_n);
    render_text(g, g->font_small, t, x, y - 20, (SDL_Color){ 200, 210, 220, 255 });
}

static void draw_chip(Gui *g, SDL_Renderer *ren, int x, int y, int w, int h,
                      const char *txt)
{
    SDL_Rect rc={x,y,w,h};
    ui_round(ren,rc,ui_surface_color((SDL_Color){226,237,248,255}),6);
    SDL_Color ink=ui_legacy_text_color(g,g->font_small,(SDL_Color){235,235,235,255});
    if(!g->ui||!g->ui->legacy_paint)ink=(SDL_Color){30,54,78,255};
    int tw=0,th=0;TTF_SizeUTF8(g->font_small,txt,&tw,&th);
    render_text_exact(g,g->font_small,txt,x+(w-(int)(tw/eff_scale(g)))/2,y+(h-text_height(g,g->font_small))/2,ink);
}

/* "<san> is best" / "<san> is a mistake" / "<san> missed the best move". */
static void move_grade_text(Gui *g,ReviewClass c, const char *san, char *out, size_t n)
{
    if(g->ui&&g->ui->zh){snprintf(out,n,"%s · %s",san,ui_old_text(g,review_name(c)));return;}
    switch (c) {
        case RC_MISS:       snprintf(out, n, "%s missed the best move", san); return;
        case RC_INACCURACY: snprintf(out, n, "%s is an inaccuracy", san); return;
        case RC_MISTAKE:    snprintf(out, n, "%s is a mistake", san); return;
        case RC_BLUNDER:    snprintf(out, n, "%s is a blunder", san); return;
        case RC_BRILLIANT:  snprintf(out, n, "%s is brilliant", san); return;
        case RC_GREAT:      snprintf(out, n, "%s is great", san); return;
        case RC_BEST:       snprintf(out, n, "%s is best", san); return;
        case RC_BOOK:       snprintf(out, n, "%s is book", san); return;
        case RC_EXCELLENT:  snprintf(out, n, "%s is excellent", san); return;
        case RC_GOOD:       snprintf(out, n, "%s is good", san); return;
        case RC_FORCED:     snprintf(out, n, "%s is forced", san); return;
        default:            snprintf(out, n, "%s", san); return;
    }
}

/* Recognize the opening of the current line (path-based) and cache it. */
static void opening_refresh(Gui *g)
{
    MoveNode *node = g->tree_cur;
    if (g->opening_cur_valid && g->opening_cur_node == node) return;
    g->opening_cur_node = node;
    g->opening_cur_valid = false;
    g->opening_eco_cur[0] = g->opening_name_cur[0] = '\0';
    if (!g->book || g->ply <= 0) return;
    /* Position-based: transpositions map to the same opening. */
    g->opening_cur_valid = opening_pos_info(g->book, &g->board,
                                            g->opening_eco_cur,
                                            sizeof g->opening_eco_cur,
                                            g->opening_name_cur,
                                            sizeof g->opening_name_cur);
}

static void render_engine_panel(Gui *g, SDL_Renderer *ren)
{
    if (g->mode != MODE_ANALYSIS) return;
    opening_refresh(g);
    bool terminal=g->state!=NO_GAME_OVER;

    /* Coach line 1: the last played move and its grade. */
    if (g->ply > 0) {
        int ly = g->board_y + 30;
        const char *san = g->move_san[g->ply - 1];
        ReviewClass lc = (g->tree_cur && g->tree_cur->parent)
                             ? (ReviewClass)g->tree_cur->cls : RC_NONE;
        if (lc == RC_NONE) lc = (ReviewClass)g->review_cls[g->ply - 1];
        bool best = (lc == RC_NONE && g->move_was_best[g->ply - 1]);
        if (lc == RC_NONE && !best) lc = RC_NONE;
        else if (best) lc = RC_BEST;

        char txt[96];
        if (lc == RC_NONE) snprintf(txt, sizeof txt, "%s", san);
        else               move_grade_text(g,lc, san, txt, sizeof txt);

        if (lc != RC_NONE)
            draw_badge_circle(g, ren, g->panel_x + 12, ly + 11, 11, lc);
        render_text(g, g->font_small, txt, g->panel_x + 28, ly,
                    (lc == RC_NONE) ? (SDL_Color){ 200, 205, 215, 255 }
                                    : (SDL_Color){ 225, 230, 220, 255 });
    }

    /* Coach line 2: score chip + the current position's best move + depth. */
    if (g->eval_ai) {
        int cy = g->board_y + 52;
        char ev[24];
        if(terminal)snprintf(ev,sizeof ev,g->state==CHECKMATE?"#":"0.00");
        else if (!g->eval_valid)
            snprintf(ev, sizeof ev, "...");
        else if (g->eval_has_mate && g->eval_mate == 0)
            snprintf(ev, sizeof ev, "#");
        else if (g->eval_has_mate)
            snprintf(ev, sizeof ev, "%sM%d", g->eval_mate > 0 ? "+" : "-",
                     g->eval_mate > 0 ? g->eval_mate : -g->eval_mate);
        else
            snprintf(ev, sizeof ev, "%+.2f", g->eval_cp / 100.0);
        draw_chip(g, ren, g->panel_x, cy, 64, 24, ev);

        if (!terminal&&g->eng_line_count > 0) {
            char fs[16];
            pv_first_san(g, g->eng_lines[0].pv, fs, sizeof fs);
            if (fs[0]) {
                draw_badge_circle(g, ren, g->panel_x + 82, cy + 12, 10, RC_BEST);
                char coach[64];
                move_grade_text(g,RC_BEST,fs,coach,sizeof coach);
                render_text(g, g->font_small, coach, g->panel_x + 98, cy + 3,
                            (SDL_Color){ 150, 205, 150, 255 });
            }
        }
        if (!terminal&&g->eval_valid&&g->eval_depth>0) {
            char dep[32];
            snprintf(dep,sizeof dep,ui_tr(g,"Depth %d","深度 %d"),g->eval_depth);
            render_text(g, g->font_small, dep, g->panel_x + 330, cy + 3,
                        (SDL_Color){ 140, 145, 155, 255 });
        }
    }

    SDL_Rect ab;
    engine_arrows_rect(g, &ab);
    ui_button(g,ab,ui_old_text(g,"Arrows"),22,g->engine_arrows);

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

    int y = t.y + 22;
    int shown = g->review_on||terminal ? 0 : 3;      /* the review bar uses this space */
    for (int i = 0; i < g->eng_line_count && i < shown; i++) {
        AiLine *L = &g->eng_lines[i];
        char sc[24], pvs[160];
        if (L->has_mate)
            snprintf(sc,sizeof sc,L->mate==0?"#":"%sM%d",(g->board.side==BLACK?-L->mate:L->mate)>=0?"+":"-",abs(L->mate));
        else
            snprintf(sc, sizeof sc, "%+.2f", (g->board.side==BLACK?-L->cp:L->cp) / 100.0);
        draw_chip(g, ren, g->panel_x, y - 2, 56, 20, sc);
        pv_to_san(g, L->pv, pvs, sizeof pvs);
        SDL_Rect line_clip={g->panel_x+64,y,PANEL_W-64,20};SDL_RenderSetClipRect(ren,&line_clip);
        render_text(g, g->font_small, pvs, g->panel_x + 64, y,
                    (SDL_Color){ 205, 215, 225, 255 });SDL_RenderSetClipRect(ren,NULL);
        y += 18;
    }

    /* Opening code + name for the current line. */
    if (g->opening_cur_valid && !g->review_on) {
        char label[128];
        if (g->opening_eco_cur[0])
            snprintf(label, sizeof label, "%s \xC2\xB7 %s", g->opening_eco_cur,
                     g->opening_name_cur);
        else
            snprintf(label, sizeof label, "%s", g->opening_name_cur);
        SDL_Rect opening_clip={g->panel_x,y+4,PANEL_W,24};SDL_RenderSetClipRect(ren,&opening_clip);
        render_text(g, g->font_small, label, g->panel_x, y + 4,
                    (SDL_Color){ 190, 200, 210, 255 });SDL_RenderSetClipRect(ren,NULL);
    }
}

/* ---- move quality badges ---- */

static const struct { ReviewClass cls; const char *label; } REVIEW_ITEMS[] = {
    { RC_BRILLIANT, "Brilliant" }, { RC_GREAT, "Great" },
    { RC_BOOK, "Book" }, { RC_BEST, "Best" },
    { RC_EXCELLENT, "Excellent" }, { RC_GOOD, "Good" },
    { RC_INACCURACY, "Inaccuracy" }, { RC_MISTAKE, "Mistake" },
    { RC_MISS, "Miss" }, { RC_BLUNDER, "Blunder" }, { RC_FORCED, "Forced" },
};

#define REVIEW_ITEM_COUNT ((int)(sizeof REVIEW_ITEMS / sizeof REVIEW_ITEMS[0]))

static bool review_badge_color(ReviewClass c, Uint8 *r, Uint8 *g, Uint8 *b)
{
    switch (c) {
        case RC_FORCED:     *r = 150; *g = 175; *b = 139; return true;
        case RC_BRILLIANT:  *r = 39;  *g = 194; *b = 163; return true;  /* teal */
        case RC_GREAT:      *r = 116; *g = 155; *b = 191; return true;  /* blue */
        case RC_BEST:       *r = 129; *g = 182; *b = 76;  return true;  /* green */
        case RC_EXCELLENT:  *r = 129; *g = 182; *b = 76;  return true;  /* green */
        case RC_GOOD:       *r = 149; *g = 183; *b = 118; return true;  /* sage */
        case RC_BOOK:       *r = 214; *g = 164; *b = 126; return true;  /* brown */
        case RC_INACCURACY: *r = 247; *g = 198; *b = 49;  return true;  /* amber */
        case RC_MISTAKE:    *r = 255; *g = 164; *b = 89;  return true;  /* orange */
        case RC_BLUNDER:    *r = 250; *g = 65;  *b = 45;  return true;  /* red */
        case RC_MISS:       *r = 255; *g = 119; *b = 105; return true;  /* coral */
        default: return false;
    }
}

static void fill_circle(SDL_Renderer *ren, int cx, int cy, int rad)
{
    for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrtf((float)(rad * rad - dy * dy));
        SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void draw_badge_circle(Gui *g, SDL_Renderer *ren, int cx, int cy, int rad,
                              ReviewClass c)
{
    Uint8 r, gg, b;
    if (!review_badge_color(c, &r, &gg, &b)) return;

    /* Vector badge texture (the circle is 3/4 of the bitmap's side). */
    if (c > 0 && c < 12 && g->badge_tex[(int)c]) {
        float side = (float)rad * 8.0f / 3.0f;
        SDL_Rect dst = { (int)lroundf(cx - side / 2.0f),
                         (int)lroundf(cy - side / 2.0f),
                         (int)lroundf(side), (int)lroundf(side) };
        SDL_RenderCopy(ren, g->badge_tex[(int)c], NULL, &dst);
        return;
    }

    const char *glyph = review_badge_glyph(c);
    if (!glyph) return;

    set_render_color(ren, 20, 20, 24);
    fill_circle(ren, cx, cy, rad + 1);
    set_render_color(ren, r, gg, b);
    fill_circle(ren, cx, cy, rad);

    int th = text_height(g, g->font_tiny);
    render_text_centered(g, g->font_tiny, glyph, cx, cy - th / 2,
                         (SDL_Color){ 255, 255, 255, 255 });
}

static void draw_quality_tint(Gui *g, SDL_Renderer *ren, int sq, ReviewClass c)
{
    Uint8 r, gg, b;
    if (!review_badge_color(c, &r, &gg, &b)) return;
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, r, gg, b, 60);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* The reference holds a large white mark over a colored square, then folds
 * that mark into the persistent corner badge. All geometry follows the board. */
static void draw_quality_celebration(Gui *g,SDL_Renderer *ren,SDL_Rect square,ReviewClass c,float elapsed)
{
    Uint8 red,green,blue;review_badge_color(c,&red,&green,&blue);
    float enter=fminf(1,elapsed/0.16f),fold=fmaxf(0,fminf(1,(elapsed-1.05f)/0.35f));
    float smooth=fold*fold*(3-2*fold),opacity=enter*(1-smooth);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,red,green,blue,(Uint8)(165*opacity));SDL_RenderFillRect(ren,&square);
    float from_x=square.x+square.w*.5f,from_y=square.y+square.h*.53f;
    float to_x=square.x+square.w-g->sq*.07f,to_y=square.y+g->sq*.07f;
    float cx=from_x+(to_x-from_x)*smooth,cy=from_y+(to_y-from_y)*smooth;
    float size=g->sq*(.70f*(1-smooth)+.36f*smooth)*(0.86f+0.14f*enter);
    if(fold>0){SDL_SetRenderDrawColor(ren,red,green,blue,(Uint8)(255*smooth));fill_circle(ren,(int)cx,(int)cy,(int)(size*.57f));}
    SDL_SetRenderDrawColor(ren,255,255,255,(Uint8)(255*enter));
    int marks=c==RC_BRILLIANT?2:1;float gap=size*.27f,width=size*.18f;
    for(int k=0;k<marks;k++){
        float x=cx+(k-(marks-1)*.5f)*gap-width/2;
        SDL_Rect stem={(int)x,(int)(cy-size*.44f),(int)fmaxf(2,width),(int)(size*.59f)};SDL_RenderFillRect(ren,&stem);
        SDL_Rect dot={(int)x,(int)(cy+size*.29f),(int)fmaxf(2,width),(int)fmaxf(2,width)};SDL_RenderFillRect(ren,&dot);
    }
    if(opacity>0.01f){
        const char *title=ui_tr(g,c==RC_BRILLIANT?"Brilliant!":"Great!",c==RC_BRILLIANT?"精彩！":"优秀！");
        int tw=0,th=0;TTF_SizeUTF8(g->font_small,title,&tw,&th);float scale=eff_scale(g);
        int width=(int)(tw/scale)+28,height=text_height(g,g->font_small)+14;
        SDL_Rect pill={clampi((int)(from_x-width/2),g->board_x+2,g->board_x+8*g->sq-width-2),clampi(square.y-height/2,g->board_y+2,g->board_y+8*g->sq-height-2),width,height};
        SDL_SetRenderDrawColor(ren,0,0,0,(Uint8)(30*opacity));SDL_Rect shadow=pill;shadow.y+=2;fill_round_rect(ren,shadow,height/2,true,true);
        SDL_SetRenderDrawColor(ren,255,255,255,(Uint8)(255*opacity));fill_round_rect(ren,pill,height/2,true,true);
        render_text_exact(g,g->font_small,title,pill.x+14,pill.y+7,(SDL_Color){red*.65f,green*.65f,blue*.65f,(Uint8)(255*opacity)});
    }
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

static void draw_quality_badge(Gui *g, SDL_Renderer *ren, int sq, ReviewClass c)
{
    SDL_Rect rc;
    window_sq(g, sq, &rc);
    if(g->mode==MODE_ANALYSIS&&(c==RC_BRILLIANT||c==RC_GREAT)&&g->anim_style!=ANIM_NONE){
        if(g->quality_class!=(int)c){g->quality_class=c;g->quality_anim=SDL_GetTicks()+1;}
        if(g->quality_anim){Uint32 now=SDL_GetTicks();float elapsed=now<g->quality_anim?0:(now-g->quality_anim)/1000.0f;
            if(elapsed<1.40f){draw_quality_celebration(g,ren,rc,c,elapsed);return;}g->quality_anim=0;
        }
    }
    int rad = g->sq * 19 / 100;
    if (rad < 9) rad = 9;
    int cx = rc.x + rc.w - g->sq * 7 / 100;
    int cy = rc.y + g->sq * 7 / 100;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 55);
    fill_circle(ren, cx, cy + 1, rad + 1);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    draw_badge_circle(g, ren, cx, cy, rad, c);
}

/* ---- move tree panel (mainline grid + inline variations, scrollable) ---- */

#define MVTOK_MAX 900

typedef struct {
    MoveNode   *node;
    ReviewClass cls;      /* for the glyph / badge */
    char        text[28];
    bool        mainline;
    int         x, y, w, ply;
} MvTok;

typedef struct {
    MvTok toks[MVTOK_MAX];
    int   n;
    int   x0, x, y;       /* flowing pen (content coordinates, scroll = 0) */
    int   right;
    int   row_h;
    int   block_x[64], block_y0[64], block_y1[64], block_n;
} MvLayout;

static MvLayout mv;

static int mv_width(Gui *g, const char *s)
{
    int w = 0, h = 0;
    if (TTF_SizeUTF8(g->font_small, s, &w, &h) != 0) return 0;
    float sc = eff_scale(g);
    return (int)lroundf((float)w / sc);
}

static int move_width(Gui *g, const char *s)
{
    int w = 0, h = 0;
    if (TTF_SizeUTF8(g->font_move, s, &w, &h) != 0) return 0;
    return (int)lroundf(w / eff_scale(g));
}

static void mv_push(MvLayout *L, MoveNode *node, ReviewClass cls, const char *text,
                    int x, int y, bool mainline)
{
    if (L->n >= MVTOK_MAX) return;
    MvTok *t = &L->toks[L->n++];
    t->node = node;
    t->ply=0;
    t->cls = cls;
    snprintf(t->text, sizeof t->text, "%s", text);
    t->x = x;
    t->y = y;
    t->mainline = mainline;
    t->w = 0;   /* filled by the caller when known */
}

static void mv_emit_flow(Gui *g, MvLayout *L, MoveNode *node, ReviewClass cls,
                         const char *text)
{
    if (L->n >= MVTOK_MAX) return;
    int badge_w = node && cls != RC_NONE ? 24 : 0;
    int w = move_width(g, text) + badge_w;
    if (L->x > L->x0 && L->x + w > L->right) {
        L->x = L->x0;
        L->y += L->row_h;
    }
    mv_push(L, node, cls, text, L->x, L->y, false);
    L->toks[L->n - 1].w = w;
    L->x += w + 7;
}

static void mv_emit_line(Gui *g, MvLayout *L, MoveNode *first, int guard);

static void mv_emit_move(Gui *g, MvLayout *L, MoveNode *m, bool force_num)
{
    int ply = mt_depth(m);
    bool white = m->parent?m->parent->board.side==WHITE:(ply%2)==1;
    int num = m->parent?m->parent->board.fullmove_number:(ply+1)/2;
    char label[32];
    if (white)
        snprintf(label, sizeof label, "%d.%s", num, m->san);
    else if (force_num)
        snprintf(label, sizeof label, "%d...%s", num, m->san);
    else
        snprintf(label, sizeof label, "%s", m->san);
    mv_emit_flow(g, L, m, (ReviewClass)m->cls, label);
}

static void mv_emit_variation(Gui *g, MvLayout *L, MoveNode *v, int guard)
{
    if (!v || guard > 8 || L->n >= MVTOK_MAX - 4) return;
    mv_emit_move(g, L, v, true);
    if (v->first) mv_emit_line(g, L, v->first, guard + 1);
}

static void mv_emit_line(Gui *g, MvLayout *L, MoveNode *first, int guard)
{
    for (MoveNode *m = first; m && guard <= 8; m = m->first) {
        mv_emit_move(g, L, m, false);
        for (MoveNode *alt = m->next; alt; alt = alt->next) {
            if (L->n >= MVTOK_MAX - 4) return;
            mv_emit_flow(g, L, NULL, RC_NONE, "(");
            mv_emit_variation(g, L, alt, guard + 1);
            mv_emit_flow(g, L, NULL, RC_NONE, ")");
        }
    }
}

static void move_box_rect(const Gui *g, SDL_Rect *r)
{
    bool analysis = (g->mode == MODE_ANALYSIS);
    r->x = g->panel_x;
    r->y = g->board_y + (analysis ? 218 : g->mode==MODE_PUZZLE ? 220 : 78);
    r->w = PANEL_W;
    int bottom = g->board_y + (analysis ? 380 : g->mode==MODE_PUZZLE ? 540 : 412);
    r->h = bottom - r->y;
    if (r->h < 60) r->h = 60;
}

static void render_move_list(Gui *g)
{
    SDL_Renderer *ren = g->ren;
    if(play_mode(g))play_sync(g);
    MoveNode *display_root=play_mode(g)?g->play_tree:g->pgntree;
    MoveNode *display_cur=play_mode(g)?g->play_view:g->tree_cur;
    bool use_tree=(g->mode==MODE_ANALYSIS||play_mode(g))&&display_root;
    SDL_Rect box;
    move_box_rect(g, &box);
    g->move_box = box;
    g->move_hit_count = 0;

    draw_rect(ren, &box, 38, 37, 34, true);

    SDL_Rect inner = { box.x + 1, box.y + 1, box.w - 2, box.h - 2 };
    SDL_RenderSetClipRect(ren, &inner);

    /* mainline chain */
    MoveNode *main[MAX_PLY];
    int mcount = 0;
    if (use_tree)
        for (MoveNode *n = display_root->first; n && mcount < MAX_PLY; n = n->first)
            main[mcount++] = n;

    mv.n = 0;
    mv.row_h = 30;
    mv.block_n = 0;

    int num_x = box.x + 6;
    int wcell = box.x + 44;
    int bcell = box.x + 232;
    int y = 8;

    bool solution_visible=g->mode==MODE_PUZZLE&&g->puzzle_solution_visible;
    char solution_san[32][8]={{0}};
    if(solution_visible){Board board;if(fen_parse(g->puzzle_cur.fen,&board))for(int k=0;k<g->puzzle_nmoves;k++){
        Move move;if(!uci_to_move(&board,g->puzzle_moves[k],&move))break;
        move_to_san(&board,move,solution_san[k],sizeof solution_san[k]);make_move_plumb(&board,move);
    }}
    int total = use_tree ? mcount : solution_visible?g->puzzle_nmoves:g->ply;
    const Board *initial=use_tree?&display_root->board:g->ply?&g->before[0]:&g->board;
    int offset=initial->side==BLACK?1:0;
    int first_number=initial->fullmove_number>0?initial->fullmove_number:1;
    int nrows = (total + offset + 1) / 2;

    for (int mvi = 0; mvi < nrows; mvi++) {
        int wi = mvi * 2-offset, bi = wi + 1;
        bool split = use_tree && wi>=0 && wi < mcount && main[wi]->next && bi < mcount;

        char num[16];
        snprintf(num, sizeof num, "%d.", mvi + first_number);
        mv_push(&mv, NULL, RC_NONE, num, num_x, y, true);

        for (int k = 0; k < 2; k++) {
            int idx = (k == 0) ? wi : bi;
            int cx = (k == 0) ? wcell : bcell;
            if (k == 1 && split) {
                mv_push(&mv, NULL, RC_NONE, "...", cx + 24, y, true);
                continue;
            }
            MoveNode *node = NULL;
            ReviewClass cls = RC_NONE;
            char text[28] = "";
            bool have = false;
            if (use_tree) {
                if (idx>=0 && idx < mcount) {
                    node = main[idx];
                    cls = (ReviewClass)node->cls;
                    snprintf(text, sizeof text, "%s", node->san);
                    have = true;
                }
            } else if(solution_visible&&idx>=0&&idx<g->puzzle_nmoves){
                snprintf(text,sizeof text,"%s",solution_san[idx]);have=solution_san[idx][0]!=0;
            } else if (idx>=0 && idx < g->ply) {
                snprintf(text, sizeof text, "%s", g->move_san[idx]);
                cls = (ReviewClass)g->review_cls[idx];
                have = true;
            }
            if (have) {
                mv_push(&mv, node, cls, text, cx + 24, y, true);
                mv.toks[mv.n - 1].w = move_width(g, text) + 28;
                mv.toks[mv.n - 1].ply=idx+1;
                mv.toks[mv.n - 1].x = cx;   /* rect starts at the badge slot */
            }
        }
        y += mv.row_h;

        if (use_tree) {
            for (int k = 0; k < 2; k++) {
                int idx = (k == 0) ? wi : bi;
                if (idx<0 || idx >= mcount) continue;
                if (k == 1 && split) {
                    mv_push(&mv, NULL, RC_NONE, num, num_x, y, true);
                    mv_push(&mv, NULL, RC_NONE, "...", wcell + 24, y, true);
                    MoveNode *black = main[bi];
                    mv_push(&mv, black, (ReviewClass)black->cls, black->san,
                            bcell, y, true);
                    mv.toks[mv.n - 1].w = move_width(g, black->san) + 28;
                    y += mv.row_h;
                }
                MoveNode *node = main[idx];
                if (!node->next) continue;
                mv.x0 = box.x + 18;
                mv.x = mv.x0;
                mv.right = box.x + box.w - 10;
                mv.y = y;
                int y0 = y;
                int bprev = mv.block_n;
                if (bprev < 64) { mv.block_x[bprev] = box.x + 8; mv.block_n = bprev + 1; }
                for (MoveNode *alt = node->next; alt; alt = alt->next) {
                    mv_emit_flow(g, &mv, NULL, RC_NONE, "(");
                    mv_emit_variation(g, &mv, alt, 0);
                    mv_emit_flow(g, &mv, NULL, RC_NONE, ")");
                }
                y = mv.y + mv.row_h;
                if (bprev < 64) { mv.block_y0[bprev] = y0; mv.block_y1[bprev] = y; }
            }
        }
    }

    int content_h = y + 6;
    g->move_scroll_max = content_h > box.h ? content_h - box.h : 0;
    if (g->move_scroll > g->move_scroll_max) g->move_scroll = g->move_scroll_max;
    if (g->move_scroll < 0) g->move_scroll = 0;

    /* auto-follow the current node */
    if (g->move_follow && (use_tree?display_cur!=NULL:true)) {
        for (int i = 0; i < mv.n; i++) {
            if (use_tree?mv.toks[i].node == display_cur:mv.toks[i].ply==(g->history_browsing?g->history_cursor:g->ply)) {
                int centre = mv.toks[i].y + mv.row_h / 2;
                int want = centre - box.h / 2;
                if (want < 0) want = 0;
                if (want > g->move_scroll_max) want = g->move_scroll_max;
                g->move_scroll = want;
                break;
            }
        }
    }

    /* Alternating mainline rows, as in 19.png. Tokens sharing y are one row. */
    int last_row = -1, row = 0;
    for (int i = 0; i < mv.n; i++) {
        MvTok *t = &mv.toks[i];
        if (!t->mainline || t->y == last_row) continue;
        last_row = t->y;
        if ((row++ % 2) == 0) {
            SDL_Rect stripe = { box.x, box.y + t->y - g->move_scroll - 3,
                                box.w, mv.row_h };
            ui_move_background(g,stripe,false);
        }
    }

    /* variation guide bars */
    set_render_color(ren, 90, 95, 105);
    for (int i = 0; i < mv.block_n; i++) {
        int bx = mv.block_x[i];
        SDL_RenderDrawLine(ren, bx, box.y + mv.block_y0[i] - g->move_scroll,
                           bx, box.y + mv.block_y1[i] - g->move_scroll);
    }

    /* tokens */
    for (int i = 0; i < mv.n; i++) {
        MvTok *t = &mv.toks[i];
        int ty = box.y + t->y - g->move_scroll;
        if (ty + mv.row_h < box.y || ty > box.y + box.h) continue;

        Uint8 r = 195, gg = 194, b = 193;
        review_badge_color(t->cls, &r, &gg, &b);
        SDL_Color mv_c = { r, gg, b, 255 };
        if ((t->node && t->node == display_cur)||(!use_tree&&t->ply>0&&t->ply==(g->history_browsing?g->history_cursor:g->ply))) {
            SDL_Rect hl = { t->x - 3, ty - 2, t->w + 6, mv.row_h };
            ui_move_background(g,hl,true);
            mv_c = (SDL_Color){255, 255, 255, 255};
        }

        if (t->mainline) {
            if (t->cls != RC_NONE)
                draw_badge_circle(g, ren, t->x + 10, ty + 10, 9,
                                  (ReviewClass)t->cls);
            if (!t->node && t->w == 0) mv_c = (SDL_Color){147, 146, 145, 255};
            render_text(g, g->font_move, t->text,
                        t->x + (t->w > 0 ? 24 : 0), ty, mv_c);
            if ((t->node||(!use_tree&&t->ply>0)) && g->move_hit_count < MAX_PLY + 64) {
                SDL_Rect hr = { t->x, ty - 2, (PANEL_W-60)/2, mv.row_h };
                g->move_hit_rect[g->move_hit_count] = hr;
                g->move_hit_ply[g->move_hit_count]=t->ply;
                g->move_hit_node[g->move_hit_count++] = t->node;
            }
        } else {
            int badge_w = t->node && t->cls != RC_NONE ? 24 : 0;
            if (badge_w)
                draw_badge_circle(g, ren, t->x + 10, ty + 10, 9, t->cls);
            render_text(g, g->font_move, t->text, t->x + badge_w, ty, mv_c);
            if ((t->node||(!use_tree&&t->ply>0)) && g->move_hit_count < MAX_PLY + 64) {
                SDL_Rect hr = { t->x - 2, ty - 2, t->w + 4, mv.row_h };
                g->move_hit_rect[g->move_hit_count] = hr;
                g->move_hit_ply[g->move_hit_count]=t->ply;
                g->move_hit_node[g->move_hit_count++] = t->node;
            }
        }
    }

    SDL_RenderSetClipRect(ren, NULL);
    ui_panel_border(ren,box,6);

    /* scrollbar */
    if (g->move_scroll_max > 0) {
        int track_h = box.h - 8;
        int bar_h = track_h * box.h / content_h;
        if (bar_h < 20) bar_h = 20;
        int bar_y = box.y + 4 + (track_h - bar_h) * g->move_scroll / g->move_scroll_max;
        SDL_Rect sb = { box.x + box.w - 6, bar_y, 4, bar_h };
        set_render_color(ren, 120, 125, 135);
        SDL_RenderFillRect(ren, &sb);
    }
}

static void render_analysis_summary(Gui *g, SDL_Renderer *ren)
{
    int x = g->panel_x, y = g->board_y;
    SDL_Rect box = { x, y + 218, PANEL_W, 358 };
    draw_rect(ren, &box, 38, 37, 34, true);
    int counts[2][12] = {{0}};
    if (g->rev_report) {
        memcpy(counts, g->rev_count_side, sizeof counts);
    } else if (g->pgntree) {
        int guard = 0;
        for (MoveNode *n = g->pgntree->first; n && guard++ < MAX_PLY; n = n->first) {
            if (n->cls > RC_NONE && n->cls <= RC_MISS)
                counts[n->parent->board.side][n->cls]++;
        }
    }
    int white_x = x + 236, badge_x = x + 310, black_x = x + 390;
    char name[24];
    snprintf(name, sizeof name, "%.13s", g->white_name);
    render_text_centered(g, g->font_small, name, white_x, y + 224,
                         (SDL_Color){235, 235, 235, 255});
    snprintf(name, sizeof name, "%.13s", g->black_name);
    render_text_centered(g, g->font_small, name, black_x, y + 224,
                         (SDL_Color){235, 235, 235, 255});
    render_text(g, g->font_move, "Accuracy", x + 14, y + 250,
                (SDL_Color){255, 255, 255, 255});
    for (int side = 0; side < 2; side++) {
        char value[20];
        if (g->rev_report) snprintf(value, sizeof value, "%.1f", g->rev_accuracy_side[side]);
        else snprintf(value, sizeof value, "--");
        draw_chip(g, ren, (side == 0 ? white_x : black_x) - 32, y + 248, 64, 24, value);
    }
    set_render_color(ren, 64, 61, 56);
    SDL_RenderDrawLine(ren, x + 12, y + 278, x + PANEL_W - 12, y + 278);
    for (int i = 0; i < REVIEW_ITEM_COUNT; i++) {
        ReviewClass c = REVIEW_ITEMS[i].cls;
        Uint8 r, gg, b;
        review_badge_color(c, &r, &gg, &b);
        int row_y = y + 284 + 26 * i;
        render_text(g, g->font_move, REVIEW_ITEMS[i].label, x + 14, row_y,
                    (SDL_Color){255, 255, 255, 255});
        draw_badge_circle(g, ren, badge_x, row_y + 10, 11, c);
        for (int side = 0; side < 2; side++) {
            char value[16];
            snprintf(value, sizeof value, "%d", counts[side][c]);
            render_text_centered(g, g->font_move, value,
                                 side == 0 ? white_x : black_x, row_y,
                                 (SDL_Color){r, gg, b, 255});
        }
    }
    g->move_hit_count = 0;
    g->move_box = (SDL_Rect){0, 0, 0, 0};
}

/* White's evaluation curve, with grade-coloured dots and a position marker.
 * Missing evaluations stay blank; they are never presented as equal scores. */
static void render_analysis_graph(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect box;
    analysis_graph_rect(g, &box);
    draw_rect(ren, &box, 64, 61, 56, true);
    MoveNode *nodes[MAX_PLY + 1];
    int n = 0;
    if (g->pgntree) {
        nodes[n++] = g->pgntree;
        for (MoveNode *m = g->pgntree->first; m && n <= MAX_PLY; m = m->first)
            nodes[n++] = m;
    }
    bool have_eval = false;
    int prev_x = 0, prev_y = 0;
    bool prev_valid = false;
    SDL_RenderSetClipRect(ren, &box);
    for (int i = 0; i < n; i++) {
        MoveNode *m = nodes[i];
        int x = box.x + (n > 1 ? i * (box.w - 1) / (n - 1) : 0);
        if (!m->has_eval) { prev_valid = false; continue; }
        have_eval = true;
        int cp = m->board.side == WHITE ? m->eval_cp : -m->eval_cp;
        int mate = m->board.side == WHITE ? m->eval_mate : -m->eval_mate;
        double pct = mate ? (mate > 0 ? 100.0 : 0.0) : review_win_pct(cp);
        int yy = box.y + 2 + (int)((100.0 - pct) * (box.h - 5) / 100.0);
        set_render_color(ren, 255, 255, 255);
        if (prev_valid && x > prev_x) {
            for (int px = prev_x; px <= x; px++) {
                int py = prev_y + (yy - prev_y) * (px - prev_x) / (x - prev_x);
                SDL_RenderDrawLine(ren, px, py, px, box.y + box.h - 1);
            }
        }
        prev_x = x; prev_y = yy; prev_valid = true;
    }
    set_render_color(ren, 130, 128, 124);
    SDL_RenderDrawLine(ren, box.x, box.y + box.h / 2, box.x + box.w, box.y + box.h / 2);
    for (int i = 0; i < n; i++) {
        MoveNode *m = nodes[i];
        int x = box.x + (n > 1 ? i * (box.w - 1) / (n - 1) : 0);
        if (m == g->tree_cur) {
            set_render_color(ren, 130, 182, 77);
            SDL_RenderDrawLine(ren, x, box.y, x, box.y + box.h);
        }
        if (!m->has_eval) continue;
        int cp = m->board.side == WHITE ? m->eval_cp : -m->eval_cp;
        int mate = m->board.side == WHITE ? m->eval_mate : -m->eval_mate;
        double pct = mate ? (mate > 0 ? 100.0 : 0.0) : review_win_pct(cp);
        int yy = box.y + 2 + (int)((100.0 - pct) * (box.h - 5) / 100.0);
        Uint8 r, gg, b;
        if (review_badge_color((ReviewClass)m->cls, &r, &gg, &b)) {
            set_render_color(ren, r, gg, b);
            fill_circle(ren, x, yy, m == g->tree_cur ? 4 : 3);
        }
    }
    if (!have_eval)
        render_text_centered(g, g->font_tiny, "Analyze to see the evaluation graph",
                             box.x + box.w / 2, box.y + 14,
                             (SDL_Color){195, 194, 193, 255});
    SDL_RenderSetClipRect(ren, NULL);
    ui_panel_border(ren,box,6);
}

static void render_san_hint(Gui *g)
{
    SDL_Rect ib;
    input_rect(g, &ib);
    render_text(g, g->font_small, "Press Space to type a move", ib.x + 12, ib.y + 12,
                (SDL_Color){ 120, 130, 140, 255 });
}

static void render_input_box(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect ib;
    input_rect(g, &ib);
    render_text(g, g->font_small, "Type a move (SAN):", ib.x, ib.y - 22,
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
        ui_caret_color(ren);
        SDL_Rect caret = { ib.x + 12 + tw, ib.y + 8, 2, ib.h - 16 };
        SDL_RenderFillRect(ren, &caret);
    } else {
        render_text(g, g->font_small, "e.g. e4, Nf3, O-O-O", ib.x + 10, ib.y + 12,
                    (SDL_Color){ 120, 120, 120, 255 });
        ui_caret_color(ren);
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
        ui_caret_color(ren);
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
    if(g->ui){
        const char *names[]={"Hint","Undo","Restart","Reveal","Next","Import","Analyze","Report","Analysis","Save PGN","Menu","Back","Start","Load","Copy","Rematch","Resign"};
        const int icons[]={1,20,19,13,16,4,2,12,2,3,18,15,23,5,21,19,24};int icon=-1;
        for(unsigned k=0;k<sizeof icons/sizeof icons[0];k++)if(!strcmp(label,names[k])){icon=icons[k];break;}
        ui_button(g,*r,ui_old_text(g,label),icon,false);return;
    }
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
    bool online_spec = g->mode == MODE_ONLINE && g->online &&
                       online_spectating(g->online);
    render_text_centered_rect(g, ren, &undo,
                              g->mode == MODE_PUZZLE ? "Hint" : "Undo");
    const char *restart_label;
    if(net_is_p2p(g->net))restart_label=p2p_finished(g)?"Rematch":"Resign";
    else if (g->mode == MODE_ONLINE)
        restart_label = online_spec ? "" : (g->online_over ? "Rematch" : "Resign");
    else if (g->mode == MODE_PUZZLE)
        restart_label = "Reveal";
    else if (g->mode == MODE_ANALYSIS)
        restart_label = g->rev_report ? "Report" : "Analyze";
    else
        restart_label = "Restart";
    render_text_centered_rect(g, ren, &restart, restart_label);
    const char *styles_label = (g->mode == MODE_PUZZLE) ? "Next"
                             : (g->mode == MODE_ANALYSIS) ? "Import" : "Styles";
    if(g->mode==MODE_PUZZLE&&!g->puzzle_done&&!g->puzzle_failed&&!g->puzzle_solution_visible)ui_disabled_button(g,styles,ui_old_text(g,"Next"));
    else if(g->mode==MODE_PUZZLE||g->mode==MODE_ANALYSIS)render_text_centered_rect(g, ren, &styles, styles_label);
    else if(ui_finished_game(g))render_text_centered_rect_f(g,ren,&styles,g->font_small,"Analysis");
    render_text_centered_rect_f(g, ren, &pgn, g->font_small, "Save PGN");
    render_text_centered_rect(g, ren, &menu, "Menu");

}

static void render_setup(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);

    render_text_centered(g, g->font_ui, "Singleplayer", (208+(g->win_w-208)/2), 150,
                         (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect sides[3], levels[3], start, back;
    setup_rects(g, sides, levels, &start, &back);

    render_text_centered(g, g->font_small, "Play as", (208+(g->win_w-208)/2), 268,
                         (SDL_Color){ 150, 180, 200, 255 });
    const char *side_labels[3] = { "White", "Black", "Random" };
    for (int i = 0; i < 3; i++) {
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

    render_text_centered(g, g->font_small, "Difficulty", (208+(g->win_w-208)/2), 388,
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
                             g->engine_install_status[0] ? g->engine_install_status : "Stockfish unavailable. Open Settings > Engine to install.",
                             (208+(g->win_w-208)/2), 610, (SDL_Color){ 255, 140, 120, 255 });
    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, (208+(g->win_w-208)/2), 640,
                             (SDL_Color){ 255, 170, 120, 255 });


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
    ui_background(g,ren);

    render_text_centered(g, g->font_ui, "Local Multiplayer", (208+(g->win_w-208)/2), 150,
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
                         (208+(g->win_w-208)/2), 560, (SDL_Color){ 140, 170, 190, 255 });

    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, (208+(g->win_w-208)/2), 600,
                             (SDL_Color){ 255, 180, 120, 255 });


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

/* Square 2x2 sample: kings on opposite corners, with aspect-preserving fit. */
static SDL_Texture *make_piece_thumb(Gui *g,const char *dir,int px)
{
    char path[THEME_PATH_MAX+16];SDL_Surface *pieces[2]={NULL,NULL};const char *names[]={"wk.png","bk.png"};
    for(int k=0;k<2;k++){snprintf(path,sizeof path,"%s/%s",dir,names[k]);pieces[k]=IMG_Load(path);}
    if(!pieces[0]||!pieces[1]){for(int k=0;k<2;k++)if(pieces[k])SDL_FreeSurface(pieces[k]);return NULL;}
    SDL_Surface *dst=SDL_CreateRGBSurfaceWithFormat(0,px,px,32,SDL_PIXELFORMAT_RGBA32);if(!dst){for(int k=0;k<2;k++)SDL_FreeSurface(pieces[k]);return NULL;}
    for(int row=0;row<2;row++)for(int col=0;col<2;col++){SDL_Rect cell={col*px/2,row*px/2,(col+1)*px/2-col*px/2,(row+1)*px/2-row*px/2};SDL_FillRect(dst,&cell,SDL_MapRGBA(dst->format,(row+col)%2?125:222,(row+col)%2?153:233,(row+col)%2?174:240,255));}
    for(int k=0;k<2;k++){SDL_Surface *piece=pieces[k];float fit=fminf(px*0.43f/piece->w,px*0.43f/piece->h);int width=lroundf(piece->w*fit),height=lroundf(piece->h*fit);SDL_Rect box={k*px/2+(px/2-width)/2,k*px/2+(px/2-height)/2,width,height};SDL_BlitScaled(piece,NULL,dst,&box);SDL_FreeSurface(piece);}
    SDL_Texture *texture=SDL_CreateTextureFromSurface(g->ren,dst);SDL_FreeSurface(dst);if(texture){SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
#endif
    }return texture;
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
                             : tab==2 ? (idx == (int)g->anim_style) : idx==g->font_index;

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
    } else if(tab==3){
        if(!g->font_preview[idx]){char path[1200];const char *file=ui_font_file(idx,path);if(file)g->font_preview[idx]=TTF_OpenFont(file,(int)lroundf(22*eff_scale(g)));}
        TTF_Font *font=g->font_preview[idx];
        if(font){render_text_centered(g,font,"Aa 棋",cell->x+cell->w/2,cell->y+30,(SDL_Color){230,230,235,255});render_text_centered(g,g->font_tiny,"e4 Nf3 O-O",cell->x+cell->w/2,cell->y+80,(SDL_Color){170,185,200,255});}
    } else {
        for (int r = 0; r < 4; r++)
            for (int f = 0; f < 4; f++) {
                SDL_Rect q = { inner.x + f * side / 4, inner.y + r * side / 4,
                               side / 4 + 1, side / 4 + 1 };
                if ((r + f) % 2 == 0) set_render_color(ren, 180, 180, 186);
                else                  set_render_color(ren, 110, 112, 120);
                SDL_RenderFillRect(ren, &q);
            }
        Uint32 elapsed=SDL_GetTicks()-g->ape_started,cycle=elapsed%1200;
        Uint32 duration=ANIM_DURS[idx];float t=duration?fmaxf(0,fminf(1,((float)cycle-160)/duration)):1;
        bool reverse=(elapsed/1200)%2;Piece piece=reverse?BN:WN;
        /* Knight move between square centres on the 4x4 preview board. */
        float fx=inner.x+side*(reverse?0.625f:0.125f),fy=inner.y+side*(reverse?0.625f:0.875f);
        float tx=inner.x+side*(reverse?0.125f:0.625f),ty=inner.y+side*(reverse?0.875f:0.625f);
        float x,y,sc,al;anim_piece_transform(g,(AnimStyle)idx,t,fx,fy,tx,ty,&x,&y,&sc,&al);
        if(idx==ANIM_ARCADE&&t>0&&t<1)draw_move_trail(ren,piece,fx,fy,tx,ty,t,side*0.25f);
        render_piece_box(g,ren,piece,x,y,side*0.20f*sc,al);
    }

    const char *label = tab==3?UI_FONT_NAMES[idx]:(tab == 2) ? ANIM_LABELS[idx]
                      : (tab == 0) ? g->boards.items[idx].label
                                   : g->pieces.items[idx].label;
    char animation_label[80];
    if(tab==2){
        if(ANIM_DURS[idx])snprintf(animation_label,sizeof animation_label,"%s · %u ms",label,ANIM_DURS[idx]);
        else snprintf(animation_label,sizeof animation_label,"%s · %s",label,ui_old_text(g,"Instant"));
        label=animation_label;
    }
    render_text_centered(g, g->font_small, label, cell->x + cell->w / 2,
                         cell->y + cell->h - 24, (SDL_Color){ 210, 215, 225, 255 });
}

static void render_appearance(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);

    render_text_centered(g, g->font_ui, "Appearance", g->win_w / 2, 30,
                         (SDL_Color){ 235, 225, 200, 255 });

    static const char *tabs[4] = { "Boards", "Pieces", "Animation", "Fonts" };
    for (int i = 0; i < 4; i++) {
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
    int page = g->ape_page[tab];
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
    if(tab<2)for(int direction=-1;direction<=1;direction+=2){
        SDL_Rect r;ape_page_rect(g,direction,&r);
        bool enabled=direction<0?page>0:page+1<pages;
        const char *label=direction<0?ui_tr(g,"‹ Previous page","‹ 上一页"):ui_tr(g,"Next page ›","下一页 ›");
        if(enabled)ui_button(g,r,label,-1,false);
        else {draw_rect(ren,&r,40,44,54,true);render_text_centered(g,g->font_small,label,r.x+r.w/2,r.y+10,(SDL_Color){110,120,130,255});}
    }
    render_text_centered(g, g->font_small, info, 208+(g->win_w-208)/2, 664,
                         (SDL_Color){ 150, 170, 190, 255 });

    SDL_Rect back;
    ape_back_rect(g, &back);
    render_text_centered_rect(g, ren, &back, "Back");


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
enum { GP_BOARD = 0, GP_PIECES, GP_ANIM, GP_MODE, GP_APPEARANCE, GP_COUNT };
static const char *GP_LABELS[GP_COUNT] = {
    "Board", "Pieces", "Animation", "Mode", "Appearance picker"
};

/* Audio & Video tab rows */
enum { AV_SOUND = 0, AV_FPS, AV_COUNT };
static const char *AV_LABELS[AV_COUNT] = { "Sound", "Max FPS" };

static const int FPS_CHOICES[] = { 0, 60, 120, 144, 240, 320 };
#define FPS_CHOICE_COUNT ((int)(sizeof FPS_CHOICES / sizeof FPS_CHOICES[0]))

static void settings_row_rect(const Gui *g, int i, SDL_Rect *r)
{
    int h = 40, gap = 14;
    *r = (SDL_Rect){ 240, 180 + i * (h + gap), g->win_w - 280, h };
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
        case GP_MODE:   snprintf(out, n, "%s", g->offline_mode ? "Offline" : "Online"); break;
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
            case GP_MODE:
                toggle_offline_mode(g);
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
    if (g->engine_install_status[0] || !g->engine_path[0]) {
        SDL_Rect install = {240, 380, 360, 40};
        draw_rect(ren, &install, 45, 65, 80, true);
        render_text(g, g->font_small,
                    g->engine_install_status[0] ? g->engine_install_status : "Install Stockfish (click)",
                    install.x + 8, install.y + 10, (SDL_Color){210, 225, 235, 255});
    }
    char cur[600];
    snprintf(cur, sizeof cur, "Current: %s",
             g->engine_path[0] ? g->engine_path : "(none)");
    render_text(g, g->font_small, cur, 240, 126, (SDL_Color){ 150, 180, 200, 255 });

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

    render_text(g, g->font_small, "Analysis settings", 240, 150,
                (SDL_Color){ 200, 200, 200, 255 });
    SDL_Rect minus[ENG_CTRL_COUNT], plus[ENG_CTRL_COUNT];
    engine_ctrl_rects(g, minus, plus);
    for (int i = 0; i < ENG_CTRL_COUNT; i++) {
        int y = minus[i].y;
        bool on = (i == g->eng_ctrl_focus);
        render_text(g, g->font_small, ENG_CTRL_LABELS[i], 240, y + 6,
                    on ? (SDL_Color){ 235, 235, 235, 255 }
                       : (SDL_Color){ 175, 185, 195, 255 });
        char val[32];
        int v = eng_ctrl_value(g, i);
        if ((i == 3 || i == 4) && v == 0) snprintf(val, sizeof val, "unlimited");
        else if (i == 0 && v == 0)        snprintf(val, sizeof val, "off");
        else                              snprintf(val, sizeof val, "%d", v);
        render_text_centered(g, g->font_small, val, 420, y + 6, (SDL_Color){ 220, 225, 235, 255 });
        draw_rect(ren, &minus[i], on ? 90 : 55, on ? 150 : 60, on ? 200 : 75, true);
        render_text_centered(g, g->font_small, "-", minus[i].x + minus[i].w / 2, y + 6,
                             (SDL_Color){ 235, 235, 235, 255 });
        draw_rect(ren, &plus[i], on ? 90 : 55, on ? 150 : 60, on ? 200 : 75, true);
        render_text_centered(g, g->font_small, "+", plus[i].x + plus[i].w / 2, y + 6,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    render_text(g, g->font_small, "Custom engine path:",
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
    ui_background(g,ren);

    render_text_centered(g, g->font_ui, "Settings", g->win_w / 2, 28,
                         (SDL_Color){ 235, 225, 200, 255 });
    if(g->ui){
        ui_button(g,(SDL_Rect){g->win_w-218,22,194,36},ui_tr(g,g->ui->dark_mode?"Dark mode: On":"Dark mode: Off",g->ui->dark_mode?"深色模式：开启":"深色模式：关闭"),-1,g->ui->dark_mode);
        ui_button(g,(SDL_Rect){g->win_w-360,22,130,36},ui_tr(g,"Themes","应用主题"),-1,false);
    }
    render_settings_tabs(g, ren);

    if (g->settings_tab == 0) render_settings_engine(g, ren);
    else                      render_settings_rows(g, ren);

    SDL_Rect back;
    settings_back_rect(g, &back);
    render_text_centered_rect(g, ren, &back, "Back");


}

/* ---- opening browser ---- */

static int openings_current(const Gui *g)
{
    return (g->opening_sel >= 0 && g->opening_sel < g->opening_match_count)
         ? g->opening_match[g->opening_sel] : -1;
}

/* Tokenize opening movetext into SAN moves (skips move numbers). */
static int opening_tokenize(const char *moves, char out[][8], int max)
{
    int n = 0;
    const char *p = moves;
    while (*p && n < max) {
        while (*p == ' ') p++;
        if (!*p) break;
        char tok[16];
        int i = 0;
        while (*p && *p != ' ' && i < 15) tok[i++] = *p++;
        tok[i] = '\0';
        if (tok[0] >= '0' && tok[0] <= '9' &&
            strspn(tok, "0123456789.") == strlen(tok)) continue;
        snprintf(out[n], 8, "%s", tok);
        n++;
    }
    return n;
}

/* Replay the opened path on the board, refresh the matching list and the
 * recognized opening (ECO + name). */
static void openings_apply_path(Gui *g)
{
    board_reset(&g->board);
    for (int i = 0; i < g->opening_path_len; i++) {
        Move m;
        if (!san_find(&g->board, g->opening_path[i], &m)) {
            g->opening_path_len = i;
            break;
        }
        make_move_plumb(&g->board, m);
    }
    g->opening_step = g->opening_path_len;
    clear_selection(g);

    /* Position-based matching: every line passing through this board (so
     * different move orders that transpose count as the same opening). */
    static int found[4096];
    int nm = opening_lines_at(g->book, &g->board, found, 4096);
    g->opening_match_count = 0;
    char eco[8], name[96], moves[256];
    for (int i = 0; i < nm && g->opening_match_count < 4096; i++) {
        if (!opening_line_at(g->book, found[i], eco, sizeof eco, name,
                             sizeof name, moves, sizeof moves)) continue;
        if (g->opening_filter_len) {
            char hay[96], nee[24];
            snprintf(hay, sizeof hay, "%s", name);
            snprintf(nee, sizeof nee, "%s", g->opening_filter);
            for (char *q = hay; *q; q++) *q = (char)tolower((unsigned char)*q);
            for (char *q = nee; *q; q++) *q = (char)tolower((unsigned char)*q);
            if (!strstr(hay, nee)) continue;
        }
        g->opening_match[g->opening_match_count++] = found[i];
    }
    if (g->opening_sel >= g->opening_match_count)
        g->opening_sel = g->opening_match_count ? g->opening_match_count - 1 : 0;
    if (g->opening_sel < 0) g->opening_sel = 0;

    /* Recognized opening from the position (ECO + name); none at the start. */
    g->opening_eco[0] = g->opening_name[0] = '\0';
    if (g->opening_path_len > 0)
        opening_pos_info(g->book, &g->board, g->opening_eco,
                         sizeof g->opening_eco, g->opening_name,
                         sizeof g->opening_name);

    /* Selected continuation (for Right stepping). */
    if (g->opening_match_count > 0) {
        int mi = g->opening_match[g->opening_sel];
        opening_line_at(g->book, mi, eco, sizeof eco, name, sizeof name,
                        g->opening_moves, sizeof g->opening_moves);
        char toks[64][8];
        g->opening_nmoves = opening_tokenize(g->opening_moves, toks, 64);
    } else {
        g->opening_moves[0] = '\0';
        g->opening_nmoves = g->opening_path_len;
    }
}

static void openings_load_selected(Gui *g)
{
    openings_apply_path(g);
}

/* Play a legal move on the board while browsing: it extends the path. */
static void openings_board_move(Gui *g, Move m)
{
    char san[16];
    g->anim_fast = false;
    enqueue_move_anim(g, &g->board, m);
    move_to_san(&g->board, m, san, sizeof san);
    if (g->opening_path_len < 63) {
        snprintf(g->opening_path[g->opening_path_len], 8, "%s", san);
        g->opening_path_len++;
    }
    openings_apply_path(g);
}

static void open_openings(Gui *g)
{
    g->scene = SCENE_OPENINGS;
    g->opening_filter_len = 0;
    g->opening_filter[0] = '\0';
    g->opening_sel = 0;
    g->opening_path_len = 0;
    g->opening_path[0][0] = '\0';
    openings_apply_path(g);
}

/* Step forward one book move of the selected line. */
static void openings_step_forward(Gui *g)
{
    int mi = openings_current(g);
    if (mi < 0) return;
    char tok[8];
    if (opening_line_next(g->book, mi, &g->board, tok, sizeof tok) == 1 &&
        g->opening_path_len < 63) {
        Move m;
        if (san_find(&g->board, tok, &m)) {
            g->anim_fast = false;
            enqueue_move_anim(g, &g->board, m);
        }
        snprintf(g->opening_path[g->opening_path_len], 8, "%s", tok);
        g->opening_path_len++;
        openings_apply_path(g);
    }
}

/* Step back one move on the board (animated in reverse). */
static void openings_step_back(Gui *g)
{
    if (g->opening_path_len <= 0) return;
    int last = g->opening_path_len - 1;

    Board pre;
    board_reset(&pre);
    for (int i = 0; i < last; i++) {
        Move mv;
        if (san_find(&pre, g->opening_path[i], &mv)) make_move_plumb(&pre, mv);
    }
    Move m;
    if (san_find(&pre, g->opening_path[last], &m)) {
        Piece piece = pre.board[MOVE_FROM(m)];
        Piece captured = EMPTY;
        int cap_sq = -1;
        if (MOVE_FLAGS(m) & FLAG_EP) {
            cap_sq = (MOVE_TO(m) & 7) | (MOVE_FROM(m) & ~7);
            captured = pre.board[cap_sq];
        } else if (pre.board[MOVE_TO(m)] != EMPTY) {
            cap_sq = MOVE_TO(m);
            captured = pre.board[cap_sq];
        }
        enqueue_anim_manual(g, piece, MOVE_TO(m), MOVE_FROM(m), captured, cap_sq, true);
    }
    g->opening_path_len--;
    openings_apply_path(g);
}

static void openings_to_analysis(Gui *g)
{
    if (g->opening_path_len <= 0) return;

    Board b;
    board_reset(&b);
    MoveNode *root = mt_new_root(&b);
    MoveNode *cur = root;

    /* Build the full selected line as the mainline. */
    const char *p = g->opening_moves;
    while (*p && mt_depth(cur) < MAX_PLY) {
        while (*p == ' ') p++;
        if (!*p) break;
        char tok[16]; int i = 0;
        while (*p && *p != ' ' && i < 15) tok[i++] = *p++;
        tok[i] = '\0';
        if (tok[0] >= '0' && tok[0] <= '9' &&
            strspn(tok, "0123456789.") == strlen(tok)) continue;
        Move m;
        if (!san_find(&cur->board, tok, &m)) break;
        cur = mt_add_child(cur, m, NULL);
    }
    /* Park the cursor at the browsed position. */
    cur = root;
    for (int i = 0; i < g->opening_path_len && cur->first; i++)
        cur = cur->first;

    if (g->pgntree) mt_free(g->pgntree);
    g->pgntree = root;
    g->tree_cur = cur;
    for (int i = 0; i < MAX_PLY; i++) {
        g->review_cls[i] = RC_NONE;
        g->review_cpl[i] = 0;
        g->review_acc[i] = 0.0;
        g->move_was_best[i] = false;
    }
    g->review_on = false;
    g->rev_report = false;
    g->opening_cur_valid = false;
    g->opening_cur_node = NULL;
    g->mode = MODE_ANALYSIS;
    g->scene = SCENE_GAME;
    g->flipped = false;
    g->auto_flip = false;
    g->saved.valid = false;
    g->override_result[0] = 0;
    sync_from_tree(g);
    snprintf(g->white_name, sizeof g->white_name, "White");
    snprintf(g->black_name, sizeof g->black_name, "Black");
    analysis_refresh(g);
    set_msg(g, "Opening loaded", NULL);
}

static void openings_rects(const Gui *g, SDL_Rect *filter, SDL_Rect *list)
{
    filter->x = g->panel_x; filter->y = g->board_y + 60;
    filter->w = PANEL_W;    filter->h = 30;
    list->x = g->panel_x;   list->y = filter->y + filter->h + 10;
    list->w = PANEL_W;      list->h = 8 * g->sq - (list->y - g->board_y) - 20;
}

static void handle_openings_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;
    if (ctrl && k == SDLK_q) { g->quit = true; return; }
    if (k == SDLK_ESCAPE) { g->scene = SCENE_MENU; return; }
    if (k == SDLK_f || (ctrl && k == SDLK_f)) {
        g->flipped = !g->flipped;
        return;
    }
    if (k == SDLK_PAGEUP) { if (g->opening_sel > 0) g->opening_sel--; openings_load_selected(g); return; }
    if (k == SDLK_PAGEDOWN) {
        if (g->opening_sel + 1 < g->opening_match_count) g->opening_sel++;
        openings_load_selected(g); return;
    }
    if (k == SDLK_UP) {openings_step_back(g);openings_step_back(g);return;}
    if (k == SDLK_DOWN) {openings_step_forward(g);openings_step_forward(g);return;}
    if (k == SDLK_LEFT)  { openings_step_back(g); return; }
    if (k == SDLK_RIGHT) { openings_step_forward(g); return; }
    if (k == SDLK_BACKSPACE || k == SDLK_DELETE) {
        if (g->opening_filter_len > 0) g->opening_filter[--g->opening_filter_len] = '\0';
        openings_apply_path(g); return;
    }
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { openings_to_analysis(g); return; }
}

static void handle_openings_textinput(Gui *g, const SDL_TextInputEvent *te)
{
    for (const char *p = te->text; *p; p++) {
        if (g->opening_filter_len >= (int)sizeof g->opening_filter - 1) break;
        if (*p >= 0x20 && *p < 0x7f) g->opening_filter[g->opening_filter_len++] = *p;
    }
    g->opening_filter[g->opening_filter_len] = '\0';
    openings_apply_path(g);
}

static void handle_openings_mousedown(Gui *g)
{
    SDL_Rect filter, list;
    openings_rects(g, &filter, &list);
    SDL_Point p = g->mouse;

    if (pt_in(&list, p.x, p.y)) {
        int row = (p.y - list.y) / 18;
        int idx = g->opening_sel + row;   /* first visible row is the selection */
        if (idx >= 0 && idx < g->opening_match_count) {
            g->opening_sel = idx;
            g->opening_path_len = 0;      /* selecting a line restarts from the top */
            openings_load_selected(g);
        }
        return;
    }

    /* Play on the board to browse the book. */
    int sq = sq_from_pos(g, p.x, p.y);
    if (sq < 0) return;
    if (g->selected >= 0) {
        for (int i = 0; i < g->targets.count; i++) {
            Move m = g->targets.moves[i];
            if (MOVE_TO(m) == sq) {
                /* promotion in the opening is rare: default to the queen */
                openings_board_move(g, m);
                return;
            }
        }
    }
    if(g->selected==sq){clear_selection(g);return;}
    Piece piece = g->board.board[sq];
    if (piece != EMPTY && ((g->board.side == WHITE) == WHITE_PIECE(piece))) {
        MoveList all;
        gen_legal(&g->board, &all);
        g->selected = sq;
        g->pick_started = SDL_GetTicks() + 1;
        g->pick_released = 0;
        g->pick_position = g->mouse;
        g->targets.count = 0;
        for (int i = 0; i < all.count; i++)
            if (MOVE_FROM(all.moves[i]) == sq && g->targets.count < MAX_MOVES)
                g->targets.moves[g->targets.count++] = all.moves[i];
    } else {
        clear_selection(g);
    }
}

/* ---- review summary report ---- */

static void open_review_report(Gui *g)
{
    g->scene = SCENE_REVIEW;
}

static void handle_review_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    if (k == SDLK_ESCAPE || k == SDLK_RETURN || k == SDLK_KP_ENTER ||
        k == SDLK_BACKSPACE)
        g->scene = SCENE_GAME;
}

static void review_back_rect(const Gui *g, SDL_Rect *r)
{
    r->w = 150; r->h = 44;
    r->x = g->win_w - 190; r->y = g->win_h - 90;
}

static void handle_review_mousedown(Gui *g)
{
    SDL_Rect back;
    review_back_rect(g, &back);
    if (pt_in(&back, g->mouse.x, g->mouse.y)) g->scene = SCENE_GAME;
}

static void report_value_box(Gui *g, SDL_Renderer *ren, int x, int y,
                             const char *txt, bool dark)
{
    SDL_Rect b = { x, y, 130, 38 };
    if (dark) draw_rect(ren, &b, 46, 48, 56, true);
    else      draw_rect(ren, &b, 236, 236, 238, true);
    render_text_centered(g, g->font_ui, txt, x + b.w / 2, y + 7,
                         dark ? (SDL_Color){ 235, 235, 235, 255 }
                              : (SDL_Color){ 30, 32, 38, 255 });
}

static void render_review_report(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);

    render_text_centered(g, g->font_ui, "Game Report", g->win_w / 2, 22,
                         (SDL_Color){ 235, 225, 200, 255 });
    if (g->opening_cur_valid) {
        char label[128];
        if (g->opening_eco_cur[0])
            snprintf(label, sizeof label, "%s \xC2\xB7 %s", g->opening_eco_cur,
                     g->opening_name_cur);
        else
            snprintf(label, sizeof label, "%s", g->opening_name_cur);
        render_text_centered(g, g->font_small, label, g->win_w / 2, 54,
                             (SDL_Color){ 150, 200, 215, 255 });
    }

    int cx = g->win_w / 2;
    int label_x = 50;
    int wcol = cx - 150;    /* White count column (right aligned) */
    int bcol = cx + 150;    /* Black count column (left aligned) */

    /* Column headers: the two players. */
    render_text_centered(g, g->font_ui, g->white_name, cx - 150, 82,
                         (SDL_Color){ 235, 235, 235, 255 });
    render_text_centered(g, g->font_ui, g->black_name, cx + 150, 82,
                         (SDL_Color){ 235, 235, 235, 255 });

    /* Accuracy and ACPL for each side. */
    render_text(g, g->font_ui, "Accuracy", label_x, 120,
                (SDL_Color){ 200, 205, 215, 255 });
    char buf[32];
    snprintf(buf, sizeof buf, "%.1f%%", g->rev_accuracy_side[0]);
    report_value_box(g, ren, cx - 215, 114, buf, false);
    snprintf(buf, sizeof buf, "%.1f%%", g->rev_accuracy_side[1]);
    report_value_box(g, ren, cx + 85, 114, buf, true);

    render_text(g, g->font_small, "Avg centipawn loss", label_x, 170,
                (SDL_Color){ 170, 175, 185, 255 });
    snprintf(buf, sizeof buf, "%d", g->rev_acpl_side[0]);
    render_text_centered(g, g->font_ui, buf, cx - 150, 166,
                         (SDL_Color){ 200, 210, 220, 255 });
    snprintf(buf, sizeof buf, "%d", g->rev_acpl_side[1]);
    render_text_centered(g, g->font_ui, buf, cx + 150, 166,
                         (SDL_Color){ 200, 210, 220, 255 });

    int n = REVIEW_ITEM_COUNT;
    int y = 214;
    for (int i = 0; i < n; i++) {
        ReviewClass c = (ReviewClass)REVIEW_ITEMS[i].cls;
        Uint8 r = 200, gg = 205, b = 215;
        review_badge_color(c, &r, &gg, &b);
        render_text(g, g->font_ui, REVIEW_ITEMS[i].label, label_x, y,
                    (SDL_Color){ 220, 225, 232, 255 });

        char cnt[16];
        snprintf(cnt, sizeof cnt, "%d", g->rev_count_side[0][c]);
        render_text_centered(g, g->font_ui, cnt, wcol, y,
                             (SDL_Color){ r, gg, b, 255 });
        draw_badge_circle(g, ren, cx, y + 11, 15, c);
        snprintf(cnt, sizeof cnt, "%d", g->rev_count_side[1][c]);
        render_text_centered(g, g->font_ui, cnt, bcol, y,
                             (SDL_Color){ r, gg, b, 255 });
        y += 38;
    }

    /* Game rating (when known). */
    bool have_rating = g->account_local ||
                       (g->online && online_logged_in(g->online));
    if (have_rating) {
        int rating = (g->online && online_logged_in(g->online))
                         ? online_pvp_rating(g->online) : g->local_rating;
        render_text(g, g->font_ui, "Game Rating", label_x, g->win_h - 96,
                    (SDL_Color){ 200, 205, 215, 255 });
        snprintf(buf, sizeof buf, "%d", rating);
        report_value_box(g, ren, cx - 215, g->win_h - 104, buf, false);
        report_value_box(g, ren, cx + 85, g->win_h - 104, buf, true);
    }

    SDL_Rect back;
    review_back_rect(g, &back);
    draw_rect(ren, &back, 50, 55, 65, true);
    draw_rect(ren, &back, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, "Back", back.x + back.w / 2,
                         back.y + 10, (SDL_Color){ 235, 235, 235, 255 });
}

static void render_openings(Gui *g, SDL_Renderer *ren)
{
    Uint32 now = SDL_GetTicks();
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);

    draw_board_squares(g, ren);
    draw_coordinates(g);
    if (g->selected >= 0) draw_selection_glow(g, ren, g->selected, now);
    for (int i = 0; i < g->targets.count; i++) {
        Move m = g->targets.moves[i];
        bool cap = (MOVE_FLAGS(m) & (FLAG_CAPTURE | FLAG_EP)) != 0;
        if(MOVE_FLAGS(m)&FLAG_PROMO)cap=false;
        draw_target_dot(g, ren, MOVE_TO(m), cap);
    }
    draw_piece_layer(g, ren, SDL_GetTicks());

    /* Check animation in the browser too. */
    {
        Uint32 now = SDL_GetTicks();
        if (g->check_anim && now - g->check_anim < 900 && g->check_anim_sq >= 0)
            draw_check_overlay(g, ren, g->check_anim_sq,
                               (float)(now - g->check_anim) / 900.0f);
    }

    render_text(g, g->font_ui, "Openings", g->panel_x, g->board_y,
                (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect filter, list;
    openings_rects(g, &filter, &list);
    draw_rect(ren, &filter, 30, 30, 34, true);
    draw_rect(ren, &filter, 90, 140, 200, false);
    render_text(g, g->font_small,
                g->opening_filter_len ? g->opening_filter : "type to search...",
                filter.x + 8, filter.y + 8,
                g->opening_filter_len ? (SDL_Color){ 230, 230, 230, 255 }
                                      : (SDL_Color){ 120, 120, 128, 255 });

    SDL_RenderSetClipRect(ren, &list);
    int rows = list.h / 18;
    char eco[8], name[96], moves[256];
    for (int r = 0; r < rows; r++) {
        int idx = g->opening_sel + r;
        if (idx >= g->opening_match_count) break;
        if (!opening_line_at(g->book, g->opening_match[idx], eco, sizeof eco,
                             name, sizeof name, moves, sizeof moves)) continue;
        int y = list.y + r * 18;
        SDL_Color c = (r == 0) ? (SDL_Color){ 245, 245, 245, 255 }
                               : (SDL_Color){ 200, 205, 215, 255 };
        if (r == 0) {
            SDL_Rect hl = { list.x - 2, y - 2, list.w + 4, 18 };
            set_render_color(ren, 70, 80, 100);
            SDL_RenderFillRect(ren, &hl);
        }
        char row[128];
        snprintf(row, sizeof row, "%s  %s", eco, name);
        render_text(g, g->font_small, row, list.x + 4, y, c);
    }
    SDL_RenderSetClipRect(ren, NULL);

    char info[200];
    if (g->opening_eco[0])
        snprintf(info, sizeof info, "%s \xC2\xB7 %s   (%d/%d)", g->opening_eco,
                 g->opening_name, g->opening_step, g->opening_nmoves);
    else
        snprintf(info, sizeof info, "Not in book   (%d/%d)", g->opening_step,
                 g->opening_nmoves);
    render_text(g, g->font_small, info, g->panel_x, list.y + list.h + 6,
                (SDL_Color){ 150, 210, 230, 255 });

}

static void render_menu(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    ui_background(g,ren);

    render_text_centered(g, g->font_piece[0], "OpenChess", g->win_w / 2, 40,
                         (SDL_Color){ 235, 225, 200, 255 });
    render_text_centered(g, g->font_small, "C / SDL chess board", g->win_w / 2,
                         40 + text_height(g, g->font_piece[0]) + 10,
                         (SDL_Color){ 130, 170, 190, 255 });

    /* Online / offline pill (click to toggle). */
    {
        SDL_Rect pill;
        menu_mode_rect(g, &pill);
        if (g->offline_mode) {
            draw_rect(ren, &pill, 60, 64, 74, true);
            draw_rect(ren, &pill, 120, 130, 150, false);
            set_render_color(ren, 150, 155, 165);
        } else {
            draw_rect(ren, &pill, 46, 122, 62, true);
            draw_rect(ren, &pill, 90, 200, 120, false);
            set_render_color(ren, 120, 230, 140);
        }
        SDL_Rect dot = { pill.x + 14, pill.y + pill.h / 2 - 5, 10, 10 };
        SDL_RenderFillRect(ren, &dot);
        render_text_centered(g, g->font_small,
                             g->offline_mode ? "Offline" : "Online",
                             pill.x + pill.w / 2 + 10, pill.y + 9,
                             (SDL_Color){ 235, 235, 238, 255 });
    }

    int mh, mgap, mstart;
    menu_metrics(g, &mh, &mgap, &mstart);
    (void)mgap; (void)mstart;
    TTF_Font *mfont = (mh < 40) ? g->font_small : g->font_ui;
    int ty_off = (mh - text_height(g, mfont)) / 2;

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
        render_text_centered(g, mfont, menu_label(g, i), g->win_w / 2,
                             r.y + ty_off, tc);
    }

    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, g->win_w / 2,
                             g->win_h - 118, (SDL_Color){ 255, 170, 120, 255 });


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
        ui_caret_color(ren);
        SDL_Rect caret = { field.x + 10 + lw, field.y + 8, 2, field.h - 16 };
        SDL_RenderFillRect(ren, &caret);
    }
    SDL_RenderSetClipRect(ren, NULL);


}

/* Online chat: recent lines above the HUD, and the entry line when active. */
static void render_chat(Gui *g, SDL_Renderer *ren)
{
    if (g->mode != MODE_ONLINE) return;
    int n = g->chat_log_n;
    for (int i = 0; i < n; i++) {
        render_text(g, g->font_small, g->chat_log[i], g->panel_x,
                    g->win_h - 340 + i * 18, (SDL_Color){ 170, 190, 205, 255 });
    }
    if (!g->chat_open) return;

    SDL_Rect ib;
    input_rect(g, &ib);
    draw_rect(ren, &ib, 30, 30, 32, true);
    draw_rect(ren, &ib, 90, 140, 200, false);
    char line[180];
    snprintf(line, sizeof line, "Chat: %s", g->chat_text);
    render_text(g, g->font_small, line, ib.x + 8, ib.y + 12,
                (SDL_Color){ 235, 235, 235, 255 });
}

static void render_game(Gui *g, SDL_Renderer *ren, Uint32 now)
{
    set_render_color(ren, 30, 30, 34);
    ui_background(g,ren);

    Board live_board=g->board,view_board;int live_ply=g->ply,live_selected=g->selected,live_targets=g->targets.count,live_anims=g->anim_count;
    GameState live_state=g->state;Uint32 live_check=g->check_anim,live_end=g->end_anim;
    bool viewing=history_board(g,&view_board);
    MoveNode *visible_node=g->mode==MODE_ANALYSIS?g->tree_cur:NULL;
    if(g->quality_node!=visible_node){g->quality_node=visible_node;g->quality_class=RC_NONE;g->quality_anim=0;}
    if(viewing){g->board=view_board;g->ply=g->history_cursor;g->state=game_state(&view_board);if(!play_branch(g)){g->selected=-1;g->targets.count=0;}g->anim_count=0;g->check_anim=g->end_anim=0;}
    draw_board_squares(g, ren);
    draw_coordinates(g);
    draw_eval_bar(g, ren);

    ReviewClass last_cls = RC_NONE;
    int last_to = -1;
    if (g->ply > 0) {
        Move m = g->history[g->ply - 1];
        if(viewing&&g->mode==MODE_PUZZLE&&g->puzzle_solution_visible){Board pre;if(fen_parse(g->puzzle_cur.fen,&pre)){for(int k=0;k<g->ply;k++){Move next;if(!uci_to_move(&pre,g->puzzle_moves[k],&next))break;m=next;make_move_plumb(&pre,next);}}}
        if(viewing&&play_branch(g)&&g->play_view->parent)m=g->play_view->move;
        last_to = MOVE_TO(m);
        last_cls = (g->mode==MODE_ANALYSIS && g->tree_cur && g->tree_cur->parent)
                       ? (ReviewClass)g->tree_cur->cls : RC_NONE;
        if (last_cls == RC_NONE) last_cls = (ReviewClass)g->review_cls[g->ply - 1];
        draw_square_highlight(g, ren, MOVE_FROM(m), 215, 215, 70);
        draw_square_highlight(g, ren, last_to, 215, 215, 70);
        if (last_cls != RC_NONE) draw_quality_tint(g, ren, last_to, last_cls);
    }

    if (in_check(&g->board, g->board.side) && g->state == NO_GAME_OVER) {
        int k = find_king(&g->board, g->board.side);
        if (k >= 0) draw_square_highlight(g, ren, k, 220, 70, 70);
    }

    if (g->selected >= 0)
        draw_selection_glow(g, ren, g->selected, now);

    if (g->selected >= 0)
        for (int i = 0; i < g->targets.count; i++) {
            Move m = g->targets.moves[i];
            bool duplicate=false;for(int j=0;j<i;j++)if(MOVE_TO(g->targets.moves[j])==MOVE_TO(m)){duplicate=true;break;}if(duplicate)continue;
            bool cap = (MOVE_FLAGS(m) & (FLAG_CAPTURE | FLAG_EP)) ? true : false;
            if (MOVE_FLAGS(m) & FLAG_PROMO) cap = false;
            draw_target_dot(g, ren, MOVE_TO(m), cap);
        }

    draw_piece_layer(g, ren, now);

    /* Check animation over the checked king's square. */
    if (g->check_anim && now - g->check_anim < 900 && g->check_anim_sq >= 0)
        draw_check_overlay(g, ren, g->check_anim_sq,
                           (float)(now - g->check_anim) / 900.0f);

    if (last_to >= 0 && last_cls != RC_NONE && g->anim_count == 0)
        draw_quality_badge(g, ren, last_to, last_cls);
    if(!viewing)draw_end_overlay(g, ren, now);
    if(g->mode==MODE_PUZZLE)ui_puzzle_feedback(g);
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

    /* The held piece and its cyan disc pulse together throughout the drag. */
    if (g->dragging && g->selected >= 0) {
        int cx = g->mouse.x, cy = g->mouse.y;

        /* Big soft radial halo behind the piece (per 17.png): a translucent
         * disc that fades out over its outer 40%. */
        int maxr = (int)(g->sq * 0.85f * pick_scale(g, now, true));
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (int rr = maxr; rr > 0; rr -= 2) {
            float f = (float)rr / (float)maxr;          /* 0 centre .. 1 edge */
            float t = f > 0.6f ? (1.0f - f) / 0.4f : 1.0f;
            int a = (int)(62.0f * t);
            SDL_SetRenderDrawColor(ren, 80, 200, 240, (Uint8)a);
            fill_circle(ren, cx, cy, rr);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        /* Cyan tint + border on the square under the cursor. */
        int sq = sq_from_pos(g, cx, cy);
        if (sq >= 0) {
            SDL_Rect rc;
            window_sq(g, sq, &rc);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, 100, 210, 245, 70);
            SDL_RenderFillRect(ren, &rc);
            set_render_color(ren, 120, 225, 250);
            SDL_Rect b = { rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2 };
            SDL_RenderDrawRect(ren, &b);
            SDL_RenderDrawRect(ren, &rc);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        }

        render_piece_box(g, ren, g->board.board[g->selected],
                         (float)g->mouse.x, (float)g->mouse.y,
                         g->sq * pick_scale(g, now, true), 1.0f);
    }

    if(viewing){g->board=live_board;g->ply=live_ply;g->state=live_state;g->selected=live_selected;g->targets.count=live_targets;g->anim_count=live_anims;g->check_anim=live_check;g->end_anim=live_end;}
    SDL_Rect activity={g->panel_x-8,g->board_y-8,PANEL_W+16,g->mode==MODE_ANALYSIS?194:g->mode==MODE_PUZZLE?62:72};
    ui_round(ren,activity,(SDL_Color){250,253,255,255},10);ui_panel_border(ren,activity,10);
    render_status(g);
    render_engine_panel(g, ren);
    render_review_bar(g, ren);
    if (g->mode == MODE_ANALYSIS && g->analysis_summary)
        render_analysis_summary(g, ren);
    else
        render_move_list(g);
    if (g->mode == MODE_ANALYSIS) {
        for (int i = 0; i < 2; i++) {
            SDL_Rect tab;
            analysis_tab_rect(g, i, &tab);
            bool selected = g->analysis_summary == (i != 0);
            draw_rect(ren, &tab, selected ? 69 : 42, selected ? 68 : 41,
                      selected ? 65 : 38, true);
            render_text_centered(g, g->font_small, i == 0 ? "Moves" : "Analysis",
                                 tab.x + tab.w / 2, tab.y + 2,
                                 (SDL_Color){235, 235, 235, 255});
            SDL_Rect r;
            analysis_nav_rect(g, i, &r);
            bool enabled = g->tree_cur && (i == 0 ? g->tree_cur->parent != NULL
                                                 : g->tree_cur->first != NULL);
            draw_rect(ren, &r, 40, 44, 52, true);
            render_text_centered(g, g->font_small,
                                 i == 0 ? "< Previous" : "Next >",
                                 r.x + r.w / 2, r.y + 4,
                                 enabled ? (SDL_Color){230, 235, 240, 255}
                                         : (SDL_Color){100, 108, 120, 255});
        }
    }
    if (g->mode == MODE_ANALYSIS) {
        if (!g->analysis_summary) {
            render_analysis_graph(g, ren);
            render_fen_box(g, ren);
        }
    }
    render_chat(g, ren);
    if(g->history_browsing){SDL_Rect field;input_rect(g,&field);char label[128];snprintf(label,sizeof label,ui_tr(g,"History %d/%d · Return to latest position","回看 %d/%d · 返回最新局面"),g->history_cursor,g->mode==MODE_PUZZLE&&g->puzzle_solution_visible?g->puzzle_nmoves:g->ply);ui_button(g,field,label,-1,false);}
    else if (!g->chat_open && !(g->mode == MODE_ANALYSIS && g->analysis_summary)) {
        if (g->san_open) render_input_box(g, ren);
        else             render_san_hint(g);
    }
    render_buttons(g, ren);

    if (g->msg[0] && SDL_GetTicks() < g->msg_until &&
        !(g->mode == MODE_ANALYSIS && g->analysis_summary)) {
        render_text(g, g->font_small, g->msg, g->panel_x, g->win_h - 150,
                    (SDL_Color){ 255, 160, 120, 255 });
    }

    render_pgn_prompt(g, ren);
    render_pgn_import(g, ren);
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

#include "p2p_game.inc"
#include "ui21.inc"
#include "p2p_ui.inc"

void gui_render(Gui *g, SDL_Renderer *ren)
{
    Uint32 now = SDL_GetTicks();
    gui_anim_advance(g, now);

    /* Keep the drawing scale in sync with the intended magnification even if
     * SDL reset it (e.g. after a window/display change). */
    apply_render_scale(g);
    ui_apply_theme(g);if(g->ui)g->ui->tooltip[0]=0;

    bool new_page=ui_render_page(g);
    if(g->ui)g->ui->legacy_paint=!new_page;
    if(new_page){}
    else if (g->scene == SCENE_MENU)
        render_menu(g, ren);
    else if (g->scene == SCENE_SINGLE_SETUP)
        render_setup(g, ren);
    else if (g->scene == SCENE_PUZZLE_SETUP)
        render_puzzle_setup(g, ren);
    else if (g->scene == SCENE_HOSTJOIN)
        render_hostjoin(g, ren);
    else if (g->scene == SCENE_P2P)
        p2p_render(g);
    else if (g->scene == SCENE_ONLINE)
        render_online(g, ren);
    else if (g->scene == SCENE_OPENINGS)
        render_openings(g, ren);
    else if (g->scene == SCENE_APPEARANCE)
        render_appearance(g, ren);
    else if (g->scene == SCENE_SETTINGS)
        render_settings(g, ren);
    else if (g->scene == SCENE_REVIEW)
        render_review_report(g, ren);
    else
        render_game(g, ren, now);

    if(g->ui){
        g->ui->legacy_paint=false;
        if(g->scene==SCENE_GAME&&g->mode==MODE_PUZZLE){ui_puzzle_strip(g);ui_daily_panel(g);ui_puzzle_rating(g);}
        if(g->scene==SCENE_GAME&&g->mode==MODE_ANALYSIS)ui_button(g,(SDL_Rect){g->board_x,g->board_y+8*g->sq+28,200,40},ui_tr(g,"Save analysis","保存分析"),3,false);
        if(g->scene==SCENE_PUZZLE_SETUP)ui_button(g,(SDL_Rect){g->board_x,g->win_h-210,310,40},ui_tr(g,g->ui->rated?"Rated · click to switch":"Unrated · click to switch",g->ui->rated?"计分练习 · 点击切换":"不计分练习 · 点击切换"),-1,false);
        ui_action_buttons(g,false);
        ui_shell(g);
        ui_modal_render(g);
    }
    if (g->account_open && g->scene != SCENE_ONLINE) {if(g->ui)g->ui->legacy_paint=true;render_account(g,ren);if(g->ui)g->ui->legacy_paint=false;}

    ui_tooltip_render(g);
    draw_debug_overlay(g, ren);
    SDL_RenderPresent(ren);
}
