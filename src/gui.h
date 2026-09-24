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

/* Default layout (also used by main.c and the tests). The live values are the
 * Gui fields board_x/board_y/sq/panel_x/win_w/win_h, which change on resize. */
#define SQ_SIZE     88
#define BOARD_X     16
#define BOARD_Y     28
#define PANEL_X     (BOARD_X + 8 * SQ_SIZE + 24)
#define PANEL_W     436
#define WIN_W       1180
#define WIN_H       760

#define MIN_SQ      44
#define MAX_SQ      150

#define MAX_PLY     1024

#define ANIM_QUEUE_MAX 16

/* Right-mouse board annotations: highlighted squares and arrows. */
#define MAX_ANN 32

typedef struct { int sq;         Uint8 r, g, b; } AnnSquare;
typedef struct { int from, to;   Uint8 r, g, b; } AnnArrow;

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
    SCENE_SETTINGS,       /* settings: engine + gameplay + audio/video */
    SCENE_GAME,
} Scene;

/* Active game mode once in SCENE_GAME. */
typedef enum {
    MODE_ANALYSIS = 0, /* one person controls both sides */
    MODE_SINGLE,       /* vs Stockfish (Phase 2) */
    MODE_LOCAL,        /* network play (Phase 3) */
} GameMode;

#define FEN_MAX 128

/* Snapshot of a game kept in memory so "Menu" can offer to continue it. Local
 * (network) games are never saved because the peer connection is gone. */
typedef struct {
    bool     valid;
    GameMode mode;
    Board    board;
    Board    before[MAX_PLY];
    Move     history[MAX_PLY];
    int      ply;
    GameState state;
    char     last_san[16];
    bool     flipped;
    bool     auto_flip;
    Color    human_color;
    int      ai_skill;
    int      ai_movetime;
    int      setup_side;
    int      setup_level;
    char     white_name[64];
    char     black_name[64];
    char     move_san[MAX_PLY][8];
} SavedGame;

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

    /* live analysis evaluation (MODE_ANALYSIS) */
    AiEngine *eval_ai;
    Color  eval_side;
    int    eval_cp;
    int    eval_mate;
    int    eval_depth;
    bool   eval_has_mate;
    bool   eval_valid;

    /* engine analysis settings (persisted) */
    int    eng_multipv;     /* 0 = engine off, 1..AI_MAX_LINES */
    int    eng_threads;     /* CPU cores */
    int    eng_hash;        /* transposition table MB */
    int    eng_time_ms;     /* 0 = unlimited */
    int    eng_depth;       /* 0 = unlimited */
    bool   engine_arrows;   /* draw the engine's best move as a green arrow */
    bool   eng_slider_drag;
    int    eng_ctrl_focus;  /* Engine screen: -1 none, 0..3 control row */
    AiLine eng_lines[AI_MAX_LINES];
    int    eng_line_count;

    /* engine picker */
    char   engine_candidates[16][512];
    int    engine_count;
    int    engine_sel;
    bool   engine_custom_focus;
    char   engine_custom[512];
    char   engine_status[128];
    Scene  engine_return_scene;
    int    settings_tab;        /* 0 = Engine, 1 = Gameplay, 2 = Audio/Video */
    int    settings_row;        /* focused row within the gameplay/AV tabs */

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
    SavedGame saved;            /* last game, resumable from the menu */
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

    /* PGN export */
    bool   pgn_prompt;
    char   pgn_name[64];
    int    pgn_len;
    char   white_name[64];
    char   black_name[64];

    /* drag feedback */
    bool   dragging;
    SDL_Point mouse;

    /* board annotations (right mouse) */
    AnnSquare ann_squares[MAX_ANN];
    int       ann_square_count;
    AnnArrow  ann_arrows[MAX_ANN];
    int       ann_arrow_count;
    bool      ann_dragging;
    int       ann_from;         /* -1 when idle */
    int       ann_to;

    SDL_Window   *win;
    SDL_Renderer *ren;
    float         ui_scale;     /* device pixels per logical point (>= 1.0) */
    bool   debug_ui;     /* OPENCHESS_DEBUG_UI: on-screen input overlay */
    int    max_fps;      /* 0 = vsync; else frame cap (60..320) */
    bool   sound;        /* play move sounds */

    /* Base layout (see DEFAULT macros). Rendering happens on this fixed canvas;
     * `zoom` magnifies the whole UI to fit the window. */
    int    sq;          /* base square size in points */
    int    board_x;
    int    board_y;
    int    panel_x;
    int    win_w;
    int    win_h;
    float  zoom;        /* whole-UI magnification (1.0 = base size) */
    float  pref_zoom;   /* user's preferred zoom (persisted; not OS-fit zoom) */
    float  pan_x, pan_y;/* temporary base-unit offset while grip-dragging */

    /* The exact transform installed by apply_render_scale. Input inverts these
     * values (not a live SDL query) so hit-testing always matches drawing. */
    float  tf_scale;    /* device pixels per base unit (density * zoom) */
    float  tf_vpx, tf_vpy;  /* viewport origin in base units */
    float  tf_ux, tf_uy;    /* window points -> device pixels */

    /* board-corner drag state */
    bool   resizing_board;
    float  resize_start_zoom;
    int    resize_start_mx;     /* raw window x/y at drag start */
    int    resize_start_my;
    int    resize_start_bx;     /* base coords grabbed on the grip */
    int    resize_start_by;
    bool   board_driven_resize; /* next window-resize event came from us */

    SDL_Point mouse_win;        /* window coordinates used for mapping */
    SDL_Point mouse_evt;        /* raw SDL event coordinates (may be stale) */
    SDL_Point mouse_global;     /* global cursor position */
    SDL_Point win_pos;          /* SDL_GetWindowPosition at the last event */

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
    TTF_Font *font_tiny;
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
void gui_apply_vsync(Gui *g);
bool gui_quit(Gui *g);

#endif
