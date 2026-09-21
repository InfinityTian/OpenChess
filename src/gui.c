#include "gui.h"
#include "pgn.h"
#include "fen.h"
#include "paths.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <SDL_image.h>

#define GUI_PI 3.14159265358979323846f

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
    float sc = g->ui_scale > 0.0f ? g->ui_scale : 1.0f;
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
    float sc = g->ui_scale > 0.0f ? g->ui_scale : 1.0f;
    int lw = (int)lroundf(w / sc);
    render_text(g, font, txt, cx - lw / 2, y, col);
}

static int window_sq(const Gui *g, int sq, SDL_Rect *out)
{
    int col, row;
    sq_screen(g, sq, &col, &row);
    out->x = BOARD_X + col * SQ_SIZE;
    out->y = BOARD_Y + row * SQ_SIZE;
    out->w = SQ_SIZE;
    out->h = SQ_SIZE;
    return 0;
}

static int sq_from_pos(const Gui *g, int px, int py)
{
    if (px < BOARD_X || py < BOARD_Y) return -1;
    int col = (px - BOARD_X) / SQ_SIZE;
    int row = (py - BOARD_Y) / SQ_SIZE;
    return screen_sq(g, col, row);
}

static bool color_of_piece(Piece p) { return WHITE_PIECE(p); }

static void piece_center(const Gui *g, int sq, float *cx, float *cy)
{
    int col, row;
    sq_screen(g, sq, &col, &row);
    *cx = BOARD_X + col * SQ_SIZE + SQ_SIZE / 2.0f;
    *cy = BOARD_Y + row * SQ_SIZE + SQ_SIZE / 2.0f;
}

static bool pt_in(const SDL_Rect *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
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
    g->promo_from = g->promo_to = -1;
    san_close_box(g);
    recompute_state(g);
    fen_refresh(g);
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

static void btn_rects(SDL_Rect *undo, SDL_Rect *restart)
{
    undo->x = PANEL_X;          undo->y = WIN_H - 120; undo->w = 96; undo->h = 40;
    restart->x = PANEL_X + 104; restart->y = WIN_H - 120; restart->w = 96; restart->h = 40;
}

static void styles_btn_rect(SDL_Rect *out)
{
    out->x = PANEL_X + 208; out->y = WIN_H - 120; out->w = 96; out->h = 40;
}

static void input_rect(SDL_Rect *out)
{
    out->x = PANEL_X; out->y = WIN_H - 200; out->w = 320; out->h = 40;
}

static void menu_btn_rect(SDL_Rect *out)
{
    out->x = PANEL_X + 312; out->y = WIN_H - 120; out->w = 100; out->h = 40;
}

/* FEN load/copy controls sit between the move list and the HUD. */
static void fen_rects(SDL_Rect *field, SDL_Rect *load, SDL_Rect *copy)
{
    field->x = PANEL_X;       field->y = BOARD_Y + 420; field->w = 252; field->h = 34;
    load->x  = PANEL_X + 260; load->y  = field->y;       load->w  = 54;  load->h  = 34;
    copy->x  = PANEL_X + 320; copy->y  = field->y;       copy->w  = 54;  copy->h  = 34;
}

/* ------------------------------------------------------------------ */
/* scenes                                                              */
/* ------------------------------------------------------------------ */

static void open_appearance(Gui *g, Scene return_to);

#define MENU_COUNT 5
static const char *MENU_ITEMS[MENU_COUNT] = {
    "Singleplayer (vs AI)",
    "Analysis (both sides)",
    "Local Multiplayer",
    "Appearance (boards & pieces)",
    "Quit",
};

/* Local Multiplayer is enabled only when SDL_net is compiled in. */
static bool menu_enabled(int i)
{
    if (i == 2) return net_available();
    return i >= 0 && i < MENU_COUNT;
}

/* Difficulty presets: engine skill (0..20) and think time in ms. */
typedef struct { const char *label; int skill; int movetime; } AiLevel;
static const AiLevel AI_LEVELS[3] = {
    { "Easy",   0,  150 },
    { "Medium", 10, 400 },
    { "Hard",   20, 1000 },
};
#define AI_LEVEL_COUNT 3

static void menu_item_rect(int i, SDL_Rect *r)
{
    r->w = 440;
    r->h = 54;
    r->x = (WIN_W - r->w) / 2;
    r->y = 300 + i * (r->h + 14);
}

static void start_analysis(Gui *g)
{
    g->scene = SCENE_GAME;
    g->mode = MODE_ANALYSIS;
    g->flipped = false;
    g->auto_flip = false;
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
}

static void go_to_menu(Gui *g)
{
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
    g->scene = SCENE_MENU;
}

static void start_single(Gui *g)
{
    g->scene = SCENE_GAME;
    g->mode = MODE_SINGLE;
    g->human_color = (g->setup_side == 1) ? BLACK : WHITE;
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
}

static void enter_local_game(Gui *g, Color side)
{
    g->scene = SCENE_GAME;
    g->mode = MODE_LOCAL;
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
}

static void promo_rects(Gui *g, SDL_Rect out[4])
{
    SDL_Rect sqr;
    window_sq(g, g->promo_to, &sqr);
    int y0 = sqr.y - 90;
    if (y0 < BOARD_Y) y0 = sqr.y + SQ_SIZE + 10;
    int x0 = sqr.x - 8;
    if (x0 < BOARD_X) x0 = sqr.x;
    if (x0 + 4 * 54 > BOARD_X + 8 * SQ_SIZE) x0 = BOARD_X + 8 * SQ_SIZE - 4 * 54;
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
    g->scene = SCENE_MENU;
    g->mode = MODE_ANALYSIS;
    g->menu_index = 1;          /* Analysis is the default selection */
    g->selected = -1;
    g->promo_from = g->promo_to = -1;
    g->input.active = false;
    g->san_open = false;
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

    /* Render at the display's native pixel density (e.g. 2x on Retina) while
     * keeping all layout and input in logical points. Textures/fonts are built
     * at `ui_scale` density and mapped back down by the render scale. */
    int ow = WIN_W, oh = WIN_H;
    SDL_GetRendererOutputSize(ren, &ow, &oh);
    float scale = (float)ow / (float)WIN_W;
    if (scale < 1.0f) scale = 1.0f;
    g->ui_scale = scale;
    SDL_RenderSetScale(ren, scale, scale);

    const char *fpath = find_font();
    if (!fpath) {
        fprintf(stderr, "no suitable font found\n");
    } else {
        g->font_piece[0] = TTF_OpenFont(fpath, (int)lroundf(SQ_SIZE * scale));
        g->font_piece[1] = TTF_OpenFont(fpath, (int)lroundf(SQ_SIZE * scale));
        g->font_ui      = TTF_OpenFont(fpath, (int)lroundf(22 * scale));
        g->font_small   = TTF_OpenFont(fpath, (int)lroundf(15 * scale));
        if (!g->font_piece[0] || !g->font_piece[1] || !g->font_ui || !g->font_small)
            fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
    }

    load_board_style(g);
    load_piece_style(g);
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
    for (int n = 0; n < MENU_COUNT; n++) {
        g->menu_index = (g->menu_index + dir + MENU_COUNT) % MENU_COUNT;
        if (menu_enabled(g->menu_index)) return;
    }
}

static void menu_activate(Gui *g)
{
    switch (g->menu_index) {
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
        case 4: g->quit = true; break;
        default:
            snprintf(g->menu_msg, sizeof g->menu_msg,
                     "'%s' is not available yet", MENU_ITEMS[g->menu_index]);
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
    for (int i = 0; i < MENU_COUNT; i++) {
        SDL_Rect r;
        menu_item_rect(i, &r);
        if (!pt_in(&r, g->mouse.x, g->mouse.y)) continue;
        g->menu_index = i;
        if (menu_enabled(i)) {
            menu_activate(g);
        } else {
            snprintf(g->menu_msg, sizeof g->menu_msg,
                     "'%s' is not available yet", MENU_ITEMS[i]);
        }
        return;
    }
}

static void handle_menu_mousemotion(Gui *g)
{
    for (int i = 0; i < MENU_COUNT; i++) {
        SDL_Rect r;
        menu_item_rect(i, &r);
        if (pt_in(&r, g->mouse.x, g->mouse.y) && menu_enabled(i)) {
            g->menu_index = i;
            return;
        }
    }
}

/* ---- singleplayer setup ---- */

static void setup_rects(SDL_Rect sides[2], SDL_Rect levels[3],
                        SDL_Rect *start, SDL_Rect *back)
{
    int bw = 120, bh = 48, gap = 20;
    int x0 = (WIN_W - (2 * bw + gap)) / 2;
    sides[0] = (SDL_Rect){ x0, 300, bw, bh };
    sides[1] = (SDL_Rect){ x0 + bw + gap, 300, bw, bh };

    int lg = 16;
    int lx = (WIN_W - (3 * bw + 2 * lg)) / 2;
    for (int i = 0; i < 3; i++)
        levels[i] = (SDL_Rect){ lx + i * (bw + lg), 420, bw, bh };

    start->w = 150; start->h = 46; start->x = WIN_W / 2 - 160; start->y = 540;
    back->w  = 150; back->h  = 46; back->x  = WIN_W / 2 + 10;  back->y  = 540;
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
    setup_rects(sides, levels, &start, &back);
    SDL_Point p = g->mouse;

    for (int i = 0; i < 2; i++)
        if (pt_in(&sides[i], p.x, p.y)) { g->setup_field = 0; g->setup_side = i; return; }
    for (int i = 0; i < 3; i++)
        if (pt_in(&levels[i], p.x, p.y)) { g->setup_field = 1; g->setup_level = i; return; }
    if (pt_in(&start, p.x, p.y)) { setup_begin(g); return; }
    if (pt_in(&back, p.x, p.y)) { g->scene = SCENE_MENU; return; }
}

/* ---- local multiplayer host/join ---- */

static void hj_rects(SDL_Rect *addr, SDL_Rect *port, SDL_Rect *host,
                     SDL_Rect *join, SDL_Rect *back)
{
    int fx = WIN_W / 2 - 200;
    addr->x = fx; addr->y = 290; addr->w = 400; addr->h = 44;
    port->x = fx; port->y = 360; port->w = 200; port->h = 44;
    host->x = WIN_W / 2 - 250; host->y = 470; host->w = 150; host->h = 48;
    join->x = WIN_W / 2 - 75;  join->y = 470; join->w = 150; join->h = 48;
    back->x = WIN_W / 2 + 100; back->y = 470; back->w = 150; back->h = 48;
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
    hj_rects(&addr, &port, &host, &join, &back);
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

static void ape_tab_rect(int i, SDL_Rect *r)
{
    int tw = 170, th = 40, gap = 20;
    int x0 = (WIN_W - (3 * tw + 2 * gap)) / 2;
    r->x = x0 + i * (tw + gap);
    r->y = 96;
    r->w = tw;
    r->h = th;
}

static void ape_cell_rect(int j, SDL_Rect *r)
{
    int cw = 170, ch = 150, gap = 12;
    int x0 = (WIN_W - (APE_COLS * cw + (APE_COLS - 1) * gap)) / 2;
    r->x = x0 + (j % APE_COLS) * (cw + gap);
    r->y = 170 + (j / APE_COLS) * (ch + gap);
    r->w = cw;
    r->h = ch;
}

static void ape_back_rect(SDL_Rect *r)
{
    r->w = 140; r->h = 42;
    r->x = WIN_W - 180; r->y = WIN_H - 70;
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
        ape_tab_rect(i, &r);
        if (pt_in(&r, p.x, p.y)) { g->ape_tab = i; return; }
    }
    SDL_Rect back;
    ape_back_rect(&back);
    if (pt_in(&back, p.x, p.y)) { g->scene = g->ape_return_scene; return; }

    int tab = g->ape_tab;
    int n = appearance_count(g, tab);
    int start = (g->ape_sel[tab] / APE_PER_PAGE) * APE_PER_PAGE;
    for (int j = 0; j < APE_PER_PAGE; j++) {
        int idx = start + j;
        if (idx >= n) break;
        SDL_Rect c;
        ape_cell_rect(j, &c);
        if (pt_in(&c, p.x, p.y)) {
            g->ape_sel[tab] = idx;
            appearance_apply(g);
            return;
        }
    }
}

/* ---- game ---- */

static void copy_fen_to_clipboard(Gui *g)
{
    char fen[FEN_MAX];
    fen_generate(&g->board, fen, sizeof fen);
    if (SDL_SetClipboardText(fen) == 0) set_msg(g, "FEN copied", NULL);
    else set_msg(g, "Clipboard unavailable", NULL);
}

static void handle_game_keydown(Gui *g, const SDL_KeyboardEvent *ke)
{
    SDL_Keycode k = ke->keysym.sym;
    bool ctrl = (ke->keysym.mod & KMOD_CTRL) != 0;

    if (k == SDLK_ESCAPE) {
        if (g->fen_active) { g->fen_active = false; fen_refresh(g); return; }
        if (g->san_open) { san_close_box(g); return; }
        clear_selection(g);
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

    SDL_Rect undo, restart, styles, menu;
    btn_rects(&undo, &restart);
    styles_btn_rect(&styles);
    menu_btn_rect(&menu);
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
    if (pt_in(&menu, p.x, p.y)) { go_to_menu(g); return; }

    /* FEN controls are analysis-only. */
    if (g->mode == MODE_ANALYSIS) {
        SDL_Rect field, load, copy;
        fen_rects(&field, &load, &copy);
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
    input_rect(&ib);
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

    if (g->promo_from >= 0) return; /* handled on down */

    int sq = sq_from_pos(g, p.x, p.y);
    if (sq < 0) return;
    if (g->selected >= 0) try_move_to(g, sq);
}

void gui_handle_event(Gui *g, const SDL_Event *e)
{
    switch (e->type) {
        case SDL_QUIT:
            g->quit = true;
            return;
        case SDL_MOUSEMOTION:
            g->mouse.x = e->motion.x;
            g->mouse.y = e->motion.y;
            if (g->scene == SCENE_MENU) { handle_menu_mousemotion(g); return; }
            if (e->motion.state & SDL_BUTTON_LMASK) {
                if (g->selected >= 0 || g->promo_from >= 0) g->dragging = true;
            }
            return;
        case SDL_MOUSEBUTTONDOWN:
            if (e->button.button != SDL_BUTTON_LEFT) return;
            g->mouse.x = e->button.x;
            g->mouse.y = e->button.y;
            if (g->scene == SCENE_MENU) handle_menu_mousedown(g);
            else if (g->scene == SCENE_SINGLE_SETUP) handle_setup_mousedown(g);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_mousedown(g);
            else if (g->scene == SCENE_APPEARANCE) handle_appearance_mousedown(g);
            else handle_game_mousedown(g);
            return;
        case SDL_MOUSEBUTTONUP:
            if (e->button.button != SDL_BUTTON_LEFT) return;
            g->mouse.x = e->button.x;
            g->mouse.y = e->button.y;
            if (g->scene == SCENE_GAME) handle_game_mouseup(g);
            return;
        case SDL_KEYDOWN:
            if (g->scene == SCENE_MENU) handle_menu_keydown(g, &e->key);
            else if (g->scene == SCENE_SINGLE_SETUP) handle_setup_keydown(g, &e->key);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_keydown(g, &e->key);
            else if (g->scene == SCENE_APPEARANCE) handle_appearance_keydown(g, &e->key);
            else handle_game_keydown(g, &e->key);
            return;
        case SDL_TEXTINPUT:
            if (g->scene == SCENE_GAME) handle_game_textinput(g, &e->text);
            else if (g->scene == SCENE_HOSTJOIN) handle_hostjoin_textinput(g, &e->text);
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

    if (g->mode == MODE_SINGLE) single_tick(g, now);
    else if (g->mode == MODE_LOCAL) local_tick(g);
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
static void anim_piece_transform(AnimStyle style, float t,
                                 float fx, float fy, float tx, float ty,
                                 float *x, float *y, float *scale, float *alpha)
{
    switch (style) {
        case ANIM_ARCADE: {
            float e = ease_out_back(t);
            *x = fx + (tx - fx) * e;
            *y = fy + (ty - fy) * e - 0.16f * SQ_SIZE * sinf(GUI_PI * t);
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
        SDL_Rect dst = { BOARD_X, BOARD_Y, 8 * SQ_SIZE, 8 * SQ_SIZE };
        SDL_RenderCopy(ren, g->tex_board, NULL, &dst);
        return;
    }
    unsigned char *light = bt ? bt->light : (unsigned char[]){ 240, 217, 181 };
    unsigned char *dark  = bt ? bt->dark  : (unsigned char[]){ 181, 136, 99 };
    for (int rank = 0; rank < 8; rank++)
        for (int file = 0; file < 8; file++) {
            SDL_Rect rc = { BOARD_X + file * SQ_SIZE, BOARD_Y + (7 - rank) * SQ_SIZE,
                            SQ_SIZE, SQ_SIZE };
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
                    BOARD_X + col * SQ_SIZE + SQ_SIZE - 14,
                    BOARD_Y + 7 * SQ_SIZE + SQ_SIZE - 20, color);
    }
    for (int row = 0; row < 8; row++) {
        int sq = screen_sq(g, 0, row);
        int file = sq % 8, rank = sq / 8;
        char c[2] = { (char)('1' + rank), 0 };
        SDL_Color color = ((file + rank) % 2 == 0) ? cdark : clite;
        render_text(g, g->font_small, c, BOARD_X + 5,
                    BOARD_Y + row * SQ_SIZE + 4, color);
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
            Piece p = g->board.board[sq];
            if (p == EMPTY) continue;
            float cx, cy;
            piece_center(g, sq, &cx, &cy);
            render_piece_box(g, ren, p, cx, cy, SQ_SIZE, 1.0f);
        }

    /* queued-but-pending moves: draw the position before those moves */
    for (int i = g->anim_count - 1; i >= 1; i--) {
        AnimStep *s = &g->anim_queue[i];
        float cx, cy;
        piece_center(g, s->from, &cx, &cy);
        render_piece_box(g, ren, s->piece, cx, cy, SQ_SIZE, 1.0f);
        if (s->cap_sq >= 0 && s->captured != EMPTY) {
            piece_center(g, s->cap_sq, &cx, &cy);
            render_piece_box(g, ren, s->captured, cx, cy, SQ_SIZE, 1.0f);
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
            render_piece_box(g, ren, s->captured, cx, cy, SQ_SIZE * sc, al);
        }

        float fx, fy, tx, ty, x, y, sc, al;
        piece_center(g, s->from, &fx, &fy);
        piece_center(g, s->to, &tx, &ty);
        anim_piece_transform(g->anim_style, t, fx, fy, tx, ty, &x, &y, &sc, &al);
        render_piece_box(g, ren, s->piece, x, y, SQ_SIZE * sc, al);

        if (s->has_rook) {
            piece_center(g, s->rfrom, &fx, &fy);
            piece_center(g, s->rto, &tx, &ty);
            anim_piece_transform(g->anim_style, t, fx, fy, tx, ty, &x, &y, &sc, &al);
            render_piece_box(g, ren, s->rook, x, y, SQ_SIZE * sc, al);
        }
    }
}

static void draw_square_highlight(Gui *gui, SDL_Renderer *ren, int sq,
                                  Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_Rect rc;
    window_sq(gui, sq, &rc);
    set_render_color(ren, r, gg, b);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(ren, &rc);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void draw_target_dot(Gui *gui, SDL_Renderer *ren, int sq, bool capture)
{
    SDL_Rect rc;
    window_sq(gui, sq, &rc);
    set_render_color(ren, 40, 90, 40);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    if (capture) {
        SDL_Rect ring = { rc.x + 4, rc.y + 4, SQ_SIZE - 8, SQ_SIZE - 8 };
        SDL_RenderDrawRect(ren, &ring);
    } else {
        int cr = 12;
        SDL_Rect dot = { rc.x + SQ_SIZE / 2 - cr, rc.y + SQ_SIZE / 2 - cr, cr * 2, cr * 2 };
        SDL_RenderFillRect(ren, &dot);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
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
    render_text(g, g->font_ui, line, PANEL_X, BOARD_Y, (SDL_Color){ 240, 220, 130, 255 });

    if (g->mode == MODE_SINGLE) {
        char info[96];
        snprintf(info, sizeof info, "You: %s    AI: %s%s",
                 g->human_color == WHITE ? "White" : "Black",
                 AI_LEVELS[g->setup_level].label,
                 g->ai_thinking ? "   (thinking...)" : "");
        render_text(g, g->font_small, info, PANEL_X, BOARD_Y + 30,
                    (SDL_Color){ 150, 210, 230, 255 });
    } else if (g->mode == MODE_LOCAL) {
        bool ok = g->net && net_state(g->net) == NET_STATE_CONNECTED;
        char info[96];
        snprintf(info, sizeof info, "You: %s    %s",
                 g->local_color == WHITE ? "White" : "Black",
                 ok ? "opponent connected" : "opponent disconnected");
        render_text(g, g->font_small, info, PANEL_X, BOARD_Y + 30,
                    (SDL_Color){ 150, 210, 230, 255 });
    }
}

static void render_move_list(Gui *g)
{
    SDL_Color c = { 230, 225, 215, 255 };
    int y = BOARD_Y + 78;
    render_text(g, g->font_small, "Moves:", PANEL_X, y - 22, c);
    int start = g->ply > 16 ? g->ply - 16 : 0;
    for (int i = start; i < g->ply; i++) {
        int num = i / 2 + 1;
        if (i % 2 == 0) {
            char header[16];
            snprintf(header, sizeof header, "%d.", num);
            render_text(g, g->font_small, header, PANEL_X, y, (SDL_Color){ 150, 150, 150, 255 });
            render_text(g, g->font_small, g->move_san[i], PANEL_X + 42, y, c);
        } else {
            render_text(g, g->font_small, g->move_san[i], PANEL_X + 150, y, c);
        }
        y += 20;
    }
}

static void render_san_hint(Gui *g)
{
    SDL_Rect ib;
    input_rect(&ib);
    render_text(g, g->font_small, "Press Enter to type a move", PANEL_X, ib.y + 12,
                (SDL_Color){ 120, 130, 140, 255 });
}

static void render_input_box(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect ib;
    input_rect(&ib);
    render_text(g, g->font_small, "Type a move (SAN):", PANEL_X, ib.y - 22,
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
            float sc = g->ui_scale > 0.0f ? g->ui_scale : 1.0f;
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
    fen_rects(&field, &load, &copy);

    render_text(g, g->font_small, "FEN:", PANEL_X, field.y - 20,
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
        float sc = g->ui_scale > 0.0f ? g->ui_scale : 1.0f;
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

static void render_text_centered_rect(Gui *g, SDL_Renderer *ren, SDL_Rect *r,
                                      const char *label)
{
    draw_rect(ren, r, 45, 45, 50, true);
    draw_rect(ren, r, 120, 120, 130, false);
    render_text_centered(g, g->font_ui, label, r->x + r->w / 2, r->y + 8,
                         (SDL_Color){ 230, 230, 230, 255 });
}

static void render_buttons(Gui *g, SDL_Renderer *ren)
{
    SDL_Rect undo, restart, styles, menu;
    btn_rects(&undo, &restart);
    styles_btn_rect(&styles);
    menu_btn_rect(&menu);
    render_text_centered_rect(g, ren, &undo, "Undo");
    render_text_centered_rect(g, ren, &restart, "Restart");
    render_text_centered_rect(g, ren, &styles, "Styles");
    render_text_centered_rect(g, ren, &menu, "Menu");
    render_text(g, g->font_small, "Ctrl+U undo   Ctrl+R restart   Ctrl+F flip", PANEL_X, undo.y + 48,
                (SDL_Color){ 130, 130, 130, 255 });
}

static void render_hud(Gui *g)
{
    char line[192];
    snprintf(line, sizeof line, "Board: %s   Pieces: %s   Anim: %s",
             theme_label(&g->boards, g->board_index),
             theme_label(&g->pieces, g->piece_index),
             ANIM_LABELS[g->anim_style]);
    render_text(g, g->font_small, line, PANEL_X, WIN_H - 268,
                (SDL_Color){ 150, 200, 215, 255 });
    render_text(g, g->font_small, "Ctrl+B board   Ctrl+P pieces   Ctrl+M anim",
                PANEL_X, WIN_H - 250, (SDL_Color){ 120, 140, 150, 255 });
}

static void render_setup(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_ui, "Singleplayer", WIN_W / 2, 150,
                         (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect sides[2], levels[3], start, back;
    setup_rects(sides, levels, &start, &back);

    render_text_centered(g, g->font_small, "Play as", WIN_W / 2, 268,
                         (SDL_Color){ 150, 180, 200, 255 });
    const char *side_labels[2] = { "White", "Black" };
    for (int i = 0; i < 2; i++) {
        bool sel = (g->setup_field == 0 && g->setup_side == i);
        draw_rect(ren, &sides[i], sel ? 70 : 40, sel ? 80 : 50, sel ? 100 : 60, true);
        if (sel) draw_rect(ren, &sides[i], 90, 150, 200, false);
        render_text_centered(g, g->font_ui, side_labels[i],
                             sides[i].x + sides[i].w / 2, sides[i].y + 12,
                             (SDL_Color){ 235, 235, 235, 255 });
    }

    render_text_centered(g, g->font_small, "Difficulty", WIN_W / 2, 388,
                         (SDL_Color){ 150, 180, 200, 255 });
    for (int i = 0; i < AI_LEVEL_COUNT; i++) {
        bool sel = (g->setup_field == 1 && g->setup_level == i);
        draw_rect(ren, &levels[i], sel ? 70 : 40, sel ? 80 : 50, sel ? 100 : 60, true);
        if (sel) draw_rect(ren, &levels[i], 90, 150, 200, false);
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
                             WIN_W / 2, 610, (SDL_Color){ 255, 140, 120, 255 });
    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, WIN_W / 2, 640,
                             (SDL_Color){ 255, 170, 120, 255 });

    render_text_centered(g, g->font_small,
                         "Left/Right change    Up/Down field    Enter start    Esc back",
                         WIN_W / 2, WIN_H - 80,
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

    render_text_centered(g, g->font_ui, "Local Multiplayer", WIN_W / 2, 150,
                         (SDL_Color){ 235, 225, 200, 255 });

    SDL_Rect addr, port, host, join, back;
    hj_rects(&addr, &port, &host, &join, &back);

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
                         WIN_W / 2, 560, (SDL_Color){ 140, 170, 190, 255 });

    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, WIN_W / 2, 600,
                             (SDL_Color){ 255, 180, 120, 255 });

    render_text_centered(g, g->font_small,
                         "Tab/Up/Down field    type to edit    Enter activate    Esc back",
                         WIN_W / 2, WIN_H - 80,
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
        anim_piece_transform((AnimStyle)idx, t, fx, fy, tx, ty, &x, &y, &sc, &al);
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

    render_text_centered(g, g->font_ui, "Appearance", WIN_W / 2, 30,
                         (SDL_Color){ 235, 225, 200, 255 });

    static const char *tabs[3] = { "Boards", "Pieces", "Animation" };
    for (int i = 0; i < 3; i++) {
        SDL_Rect r;
        ape_tab_rect(i, &r);
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
        ape_cell_rect(j, &c);
        draw_ape_cell(g, ren, tab, idx, &c, idx == sel);
    }

    int pages = (n + APE_PER_PAGE - 1) / APE_PER_PAGE;
    if (pages < 1) pages = 1;
    char info[64];
    snprintf(info, sizeof info, "%d / %d    page %d/%d", sel + 1, n, page + 1, pages);
    render_text_centered(g, g->font_small, info, WIN_W / 2, 662,
                         (SDL_Color){ 150, 170, 190, 255 });

    SDL_Rect back;
    ape_back_rect(&back);
    render_text_centered_rect(g, ren, &back, "Back");

    render_text_centered(g, g->font_small,
                         "Arrows select    Tab / 1-2-3 switch tab    Enter apply    Esc back",
                         WIN_W / 2, WIN_H - 40,
                         (SDL_Color){ 110, 120, 130, 255 });
}

static void render_menu(Gui *g, SDL_Renderer *ren)
{
    set_render_color(ren, 22, 26, 34);
    SDL_RenderClear(ren);

    render_text_centered(g, g->font_piece[0], "Chess", WIN_W / 2, 130,
                         (SDL_Color){ 235, 225, 200, 255 });
    render_text_centered(g, g->font_small, "C / SDL chess board", WIN_W / 2, 236,
                         (SDL_Color){ 130, 170, 190, 255 });

    for (int i = 0; i < MENU_COUNT; i++) {
        SDL_Rect r;
        menu_item_rect(i, &r);
        bool sel = (i == g->menu_index);
        bool en = menu_enabled(i);

        if (en) {
            Uint8 base = sel ? 70 : 40;
            set_render_color(ren, base, base + 10, base + 20);
            SDL_RenderFillRect(ren, &r);
            if (sel) draw_rect(ren, &r, 90, 150, 200, false);
        }

        SDL_Color tc = !en ? (SDL_Color){ 100, 105, 115, 255 }
                      : sel ? (SDL_Color){ 245, 245, 245, 255 }
                            : (SDL_Color){ 205, 210, 220, 255 };
        render_text_centered(g, g->font_ui, MENU_ITEMS[i], WIN_W / 2, r.y + 13, tc);
    }

    if (g->menu_msg[0])
        render_text_centered(g, g->font_small, g->menu_msg, WIN_W / 2, WIN_H - 150,
                             (SDL_Color){ 255, 170, 120, 255 });

    render_text_centered(g, g->font_small,
                         "Up/Down select    Enter choose    Ctrl+Q quit",
                         WIN_W / 2, WIN_H - 80,
                         (SDL_Color){ 110, 120, 130, 255 });
}

static void render_game(Gui *g, SDL_Renderer *ren, Uint32 now)
{
    set_render_color(ren, 30, 30, 34);
    SDL_RenderClear(ren);

    draw_board_squares(g, ren);
    draw_coordinates(g);

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

    if (g->promo_from >= 0) draw_promo_chooser(g, ren);

    if (g->dragging && g->selected >= 0) {
        render_piece_box(g, ren, g->board.board[g->selected],
                         (float)g->mouse.x, (float)g->mouse.y, SQ_SIZE + 18, 1.0f);
        SDL_Rect rc;
        window_sq(g, g->selected, &rc);
        set_render_color(ren, 0, 0, 0);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(ren, &rc);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    render_status(g);
    render_move_list(g);
    if (g->mode == MODE_ANALYSIS) render_fen_box(g, ren);
    render_hud(g);
    if (g->san_open) render_input_box(g, ren);
    else             render_san_hint(g);
    render_buttons(g, ren);

    if (g->msg[0] && SDL_GetTicks() < g->msg_until) {
        render_text(g, g->font_small, g->msg, PANEL_X, WIN_H - 150,
                    (SDL_Color){ 255, 160, 120, 255 });
    }
}

void gui_render(Gui *g, SDL_Renderer *ren)
{
    Uint32 now = SDL_GetTicks();
    gui_anim_advance(g, now);

    if (g->scene == SCENE_MENU)
        render_menu(g, ren);
    else if (g->scene == SCENE_SINGLE_SETUP)
        render_setup(g, ren);
    else if (g->scene == SCENE_HOSTJOIN)
        render_hostjoin(g, ren);
    else if (g->scene == SCENE_APPEARANCE)
        render_appearance(g, ren);
    else
        render_game(g, ren, now);

    SDL_RenderPresent(ren);
}
