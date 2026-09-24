#ifndef ONLINE_H
#define ONLINE_H

#include "transport.h"
#include "board.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * Online multiplayer session: owns a WebSocket transport and speaks protocol v2
 * (see proto.h). The GUI drives it with online_poll() each frame and reacts to
 * the events it emits; it never touches JSON or the transport directly.
 */

typedef enum {
    ONLINE_CONNECTING = 0,
    ONLINE_IDLE,        /* connected, in the lobby */
    ONLINE_HOSTING,     /* private room created, waiting for a join */
    ONLINE_QUEUED,      /* waiting in the matchmaking queue */
    ONLINE_PLAYING,
    ONLINE_CLOSED,      /* connection lost/refused */
} OnlineState;

typedef enum {
    ONLINE_EV_NONE = 0,
    ONLINE_EV_START,          /* game began; online_color() is set */
    ONLINE_EV_MOVE,           /* move in uci/ply (apply when ply == local ply) */
    ONLINE_EV_STATE,          /* authoritative resync; see online_fen() */
    ONLINE_EV_REJECT,         /* last action rejected; see online_last_error() */
    ONLINE_EV_GAMEOVER,       /* result/reason available */
    ONLINE_EV_OPPONENT_LEFT,
    ONLINE_EV_OPPONENT_JOINED,
    ONLINE_EV_REMATCH,        /* opponent requested a rematch */
    ONLINE_EV_DRAW_OFFER,     /* opponent offered a draw */
    ONLINE_EV_DRAW_DECLINE,   /* opponent declined our draw offer */
    ONLINE_EV_SPECTATE,       /* joined a game as a spectator */
} OnlineEvent;

typedef struct OnlineSession OnlineSession;

/* `resume_token` is the token from a previous session (may be NULL/empty); when
 * it matches a disconnected seat, the server restores that game. */
OnlineSession *online_create(const char *url, const char *nick,
                             const char *resume_token);
void online_destroy(OnlineSession *o);

/* Pump the socket and dispatch incoming messages. Call once per frame. */
void online_poll(OnlineSession *o);

/* True while an automatic reconnect attempt is in progress. */
bool online_reconnecting(const OnlineSession *o);

OnlineState online_state(const OnlineSession *o);
const char *online_room_code(const OnlineSession *o);
const char *online_opponent(const OnlineSession *o);
const char *online_last_error(const OnlineSession *o);
const char *online_result(const OnlineSession *o);   /* e.g. "1-0", "" */
Color       online_color(const OnlineSession *o);    /* local side */

/* Lobby actions. `time_ms` is the base clock (0 = unlimited), `inc_ms` the
 * per-move increment; both are only hints until the server assigns sides. */
void online_create_room(OnlineSession *o, int time_ms, int inc_ms);
void online_join_room(OnlineSession *o, const char *code);
void online_queue(OnlineSession *o, int time_ms, int inc_ms);
void online_cancel_queue(OnlineSession *o);

/* Latest authoritative position, ply and clock state. */
const char *online_fen(const OnlineSession *o);
int         online_ply(const OnlineSession *o);
bool online_has_clocks(const OnlineSession *o);
void online_clocks(const OnlineSession *o, int *white_ms, int *black_ms);

/* In-game actions. */
void online_send_move(OnlineSession *o, const char *uci, int ply);
void online_resign(OnlineSession *o);
void online_rematch(OnlineSession *o);
void online_draw_offer(OnlineSession *o);
void online_draw_accept(OnlineSession *o);
void online_draw_decline(OnlineSession *o);
void online_send_chat(OnlineSession *o, const char *text);

/* Spectate a running game by room code (read-only). */
void online_spectate(OnlineSession *o, const char *code);
bool online_spectating(const OnlineSession *o);

/* Pop one chat line; returns false when the queue is empty. */
bool online_take_chat(OnlineSession *o, char *from, size_t from_cap,
                      char *text, size_t text_cap);

/* Pop one event; for ONLINE_EV_MOVE fills uci/ply. */
OnlineEvent online_next_event(OnlineSession *o, char *uci, size_t cap, int *ply);

#endif
