#ifndef GUI_H
#define GUI_H

#include "board.h"
#include "move.h"
#include "ai.h"
#include "net.h"
#include "themes.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdbool.h>

#define SQ_SIZE     88
#define BOARD_X     16
#define BOARD_Y     28
#define PANEL_X     (BOARD_X + 8 * SQ_SIZE + 24)
#define WIN_W       1180
#define WIN_H       760

#define MAX_PLY     1024

#define ANIM_QUEUE_MAX 16

typedef struct {
    char   text[64];
    int    len;
    bool   active;
} InputBox;

/* Board and piece themes are enumerated at runtime from assets/themes.txt
 * (see themes.h); the user selects indices into Gui.boards / Gui.pieces. */

typedef enum {
    ANIM_ARCADE = 0,   /* springy overshoot + lift + scale pop */
    ANIM_SLIDE,
    ANIM_FADE,
    ANIM_NONE,
    ANIM_STYLE_COUNT
} AnimStyle;

/* Top-level screens. */
typedef enum {
    SCENE_MENU = 0,       /* welcome / game-mode selection */
    SCENE_SINGLE_SETUP,   /* choose side + difficulty */
    SCENE_HOSTJOIN,       /* local multiplayer host/join */
    SCENE_APPEARANCE,     /* board/piece/animation picker */
    SCENE_GAME,
} Scene;

/* Active game mode once in SCENE_GAME. */
typedef enum {
    MODE_ANALYSIS = 0, /* one person controls both sides */
    MODE_SINGLE,       /* vs Stockfish (Phase 2) */
    MODE_LOCAL,        /* network play (Phase 3) */
} GameMode;

#define FEN_MAX 128

/* One queued visual move. Board state already reflects every queued move; the
 * queue only describes how to draw the transition. */
typedef struct {
    Piece  piece;
    int    from, to;
    Piece  captured;        /* EMPTY when nothing was captured */
    int    cap_sq;          /* square the captured piece is drawn on (-1 none) */
    bool   has_rook;        /* castling rook also moves */
    Piece  rook;
    int    rfrom, rto;
    Uint32 start;
    Uint32 dur;
    bool   started;
} AnimStep;

typedef struct {
    Scene    scene;
    GameMode mode;

    Board  board;
    Board  before[MAX_PLY];     /* position before move ply */
    Move   history[MAX_PLY];    /* applied moves */
    int    ply;
    GameState state;
    char   last_san[16];

    /* board orientation */
    bool   flipped;             /* black at the bottom when true */
    bool   auto_flip;           /* orient to the local side automatically */

    /* welcome / mode menu */
    int    menu_index;
    char   menu_msg[96];

    /* singleplayer (Stockfish) setup + state */
    int    setup_field;         /* 0 = side, 1 = difficulty */
    int    setup_side;          /* 0 = white, 1 = black */
    int    setup_level;         /* index into AI_LEVELS */
    char   engine_path[512];
    AiEngine *ai;
    Color  human_color;
    int    ai_skill;
    int    ai_movetime;
    bool   ai_thinking;
    Uint32 ai_think_start;

    /* local multiplayer */
    Net   *net;
    Color  local_color;
    int    net_sent_ply;        /* plies already transmitted to the peer */
    bool   net_waiting;         /* host is waiting for a peer */
    int    hj_focus;            /* host/join screen focus index */
    char   net_addr[64];
    char   net_port[8];

    int    selected;            /* from-square, or -1 */
    MoveList targets;           /* legal targets when a piece is selected */

    char   move_san[MAX_PLY][8];/* SAN per ply for the move list */
    InputBox input;
    bool   san_open;            /* SAN entry revealed (Enter to open) */

    /* FEN field (analysis): editable while focused, refreshed otherwise */
    bool   fen_active;
    char   fen_text[FEN_MAX];
    int    fen_len;

    /* promotion chooser state */
    int    promo_from, promo_to;/* -1 when inactive */
    char   msg[160];
    Uint32 msg_until;

    /* drag feedback */
    bool   dragging;
    SDL_Point mouse;

    SDL_Renderer *ren;
    float         ui_scale;     /* device pixels per logical point (>= 1.0) */

    /* runtime theme lists + current selection */
    ThemeList boards;
    ThemeList pieces;
    int       board_index;
    int       piece_index;
    AnimStyle anim_style;

    /* appearance picker (thumbnails are built lazily and cached) */
    int      ape_tab;           /* 0 = boards, 1 = pieces, 2 = animation */
    int      ape_sel[3];        /* selected item per tab */
    Uint32   ape_started;       /* for the animation preview loop */
    Scene    ape_return_scene;  /* where Back returns to */
    SDL_Texture **board_thumbs; /* per board theme, lazily created */
    SDL_Texture **piece_thumbs; /* per piece theme, lazily created */

    /* configuration persistence */
    char   config_path[512];
    bool   config_dirty;

    /* move animation queue (front = currently animating) */
    AnimStep anim_queue[ANIM_QUEUE_MAX];
    int      anim_count;

    TTF_Font *font_piece[2];    /* [0] filled glyph, [1] outline via same */
    TTF_Font *font_ui;
    TTF_Font *font_small;
    SDL_Texture *tex_board;     /* active board image, or NULL for procedural */
    SDL_Texture *tex_piece[16]; /* piece textures (indexed by Piece) */
    bool   quit;
} Gui;

Gui *gui_create(void);
void gui_init_assets(Gui *g, SDL_Renderer *ren);
void gui_load_config(Gui *g, const char *path);
void gui_save_config(Gui *g);
void gui_destroy(Gui *g);
void gui_handle_event(Gui *g, const SDL_Event *e);
void gui_tick(Gui *g, Uint32 now);
void gui_render(Gui *g, SDL_Renderer *ren);
void gui_anim_advance(Gui *g, Uint32 now);
void gui_cycle_board(Gui *g);
void gui_cycle_pieces(Gui *g);
void gui_cycle_anim(Gui *g);
bool gui_quit(Gui *g);

#endif
