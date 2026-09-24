#ifndef PROTO_H
#define PROTO_H

#include "cJSON.h"
#include <stdbool.h>

/*
 * Online protocol v2 message types (JSON frames over WebSocket), plus small
 * cJSON helpers shared by the client (net_ws/online) and the server.
 *
 * The LAN transport keeps its own simple line protocol (HELLO/FEN/MOVE/BYE).
 */

#define PROTO_VERSION 2

typedef enum {
    PROTO_MSG_UNKNOWN = 0,

    /* client -> server */
    PROTO_C2S_HELLO,
    PROTO_C2S_CREATE,
    PROTO_C2S_JOIN,
    PROTO_C2S_QUEUE,
    PROTO_C2S_CANCEL_QUEUE,
    PROTO_C2S_MOVE,
    PROTO_C2S_RESIGN,
    PROTO_C2S_DRAW_OFFER,
    PROTO_C2S_DRAW_ACCEPT,
    PROTO_C2S_DRAW_DECLINE,
    PROTO_C2S_REMATCH,
    PROTO_C2S_CHAT,
    PROTO_C2S_PING,
    PROTO_C2S_PONG,
    PROTO_C2S_SPECTATE,

    /* server -> client */
    PROTO_S2C_WELCOME,
    PROTO_S2C_QUEUED,
    PROTO_S2C_ROOM,
    PROTO_S2C_START,
    PROTO_S2C_MOVE,
    PROTO_S2C_REJECT,
    PROTO_S2C_STATE,
    PROTO_S2C_GAMEOVER,
    PROTO_S2C_OPPONENT,
    PROTO_S2C_CHAT,
    PROTO_S2C_PONG,
    PROTO_S2C_PING,
    PROTO_S2C_REMATCH,
    PROTO_S2C_DRAW,
    PROTO_S2C_SPECTATE,

    PROTO_MSG_COUNT
} ProtoMsg;

/* Wire name for a message type ("hello", "move", ...); "" when unknown. */
const char *proto_msg_name(ProtoMsg m);

/* Parse a wire name; PROTO_MSG_UNKNOWN when it does not match. */
ProtoMsg proto_msg_from_name(const char *name);

/* ---- framing helpers ---------------------------------------------------- */

/* A parsed frame: `root` is owned by the frame until proto_frame_free(). */
typedef struct {
    ProtoMsg type;
    cJSON   *root;
} ProtoFrame;

/* Direction for parsing: some wire names (move, chat) exist both ways, so the
 * caller must say which side it is on to get the right enum value. */
typedef enum { PROTO_DIR_ANY = 0, PROTO_DIR_C2S, PROTO_DIR_S2C } ProtoDir;

/* Parse a JSON text frame. False on invalid JSON or unknown "type". */
bool proto_parse_dir(const char *text, ProtoDir dir, ProtoFrame *out);
bool proto_parse(const char *text, ProtoFrame *out);
void proto_frame_free(ProtoFrame *f);

/* NULL-safe field access on a parsed frame. */
const char *proto_field_str(const ProtoFrame *f, const char *key);
int         proto_field_int(const ProtoFrame *f, const char *key, int def);
bool        proto_field_bool(const ProtoFrame *f, const char *key, bool def);

/* Create {"type": <name>} (hello also gets "v"). Caller adds fields. */
cJSON *proto_new(ProtoMsg type);

/* Serialize a message to a heap string and free it. Caller frees the result. */
char *proto_serialize(cJSON *msg);

#endif
