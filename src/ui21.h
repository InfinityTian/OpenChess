#ifndef UI21_H
#define UI21_H
#include "workspace.h"
#include "position.h"
#include <SDL.h>
typedef struct {
    Puzzle puzzle;
    char attempt[80], played[256];
    int day, step, delta, elapsed;
    bool rated, hint, reveal, settled, solved, pending;
} PuzzleEntry;
typedef struct Ui21 {
    Workspace *store, *cache;
    char owner[96], nickname[100], status[256], input[8192], search[100];
    bool zh, rated, smart, reviewing, legacy_paint, input_focus, calendar_open;
    bool background_fit;
    int background_mask, avatar, nav_hover, nav_focus, page, social_tab, peer, offset;
    int selected_day, month_day, daily_current, daily_longest, rating_class, rating_span;
    int current_entry, suspended_entry, history_scroll, opening_preview;
    Uint32 entry_started, last_refresh;
    PuzzleEntry *entries;
    int entry_count,entry_capacity;
    Puzzle preview;
    bool preview_valid;
    PositionEditor position;
    bool setup_fen_focus;
    int setup_control;
    char setup_fen[256];
    Board analysis_preview;
    SDL_Texture *logo, *background;
    char background_file[1200];
    cJSON *people,*messages,*profile,*rating_history,*games,*calendar,*invitations,*headtohead;
    int self_id;
    cJSON *analyses;
    bool connected_before, challenge_open, challenge_rated, preview_requested, local_game_saved;
    int challenge_time,challenge_inc,challenge_color,message_scroll,message_scroll_max;
    char local_game_id[80],composition[128];
    cJSON *friends;
    int unread_messages,friend_requests;
    bool metadata_open;
    int dialog_field;
    char challenge_minutes[16],challenge_increment[16];
    char metadata[6][128];
    bool erase_tool;
    int home_scroll, message_cursor;
} Ui21;
#endif
