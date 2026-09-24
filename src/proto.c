#include "proto.h"
#include <stdlib.h>
#include <string.h>

static const char *const PROTO_NAMES[PROTO_MSG_COUNT] = {
    [PROTO_MSG_UNKNOWN]     = "",
    [PROTO_C2S_HELLO]       = "hello",
    [PROTO_C2S_CREATE]      = "create",
    [PROTO_C2S_JOIN]        = "join",
    [PROTO_C2S_QUEUE]       = "queue",
    [PROTO_C2S_CANCEL_QUEUE]= "cancel_queue",
    [PROTO_C2S_MOVE]        = "move",
    [PROTO_C2S_RESIGN]      = "resign",
    [PROTO_C2S_DRAW_OFFER]  = "draw_offer",
    [PROTO_C2S_DRAW_ACCEPT] = "draw_accept",
    [PROTO_C2S_DRAW_DECLINE]= "draw_decline",
    [PROTO_C2S_REMATCH]     = "rematch",
    [PROTO_C2S_CHAT]        = "chat",
    [PROTO_C2S_PING]        = "ping",
    [PROTO_C2S_PONG]        = "pong",
    [PROTO_C2S_SPECTATE]    = "spectate",
    [PROTO_C2S_REGISTER]    = "register",
    [PROTO_C2S_LOGIN]       = "login",
    [PROTO_C2S_LOGOUT]      = "logout",
    [PROTO_C2S_PUZZLE_RESULT] = "puzzle_result",
    [PROTO_S2C_WELCOME]     = "welcome",
    [PROTO_S2C_QUEUED]      = "queued",
    [PROTO_S2C_ROOM]        = "room",
    [PROTO_S2C_START]       = "start",
    [PROTO_S2C_MOVE]        = "move",
    [PROTO_S2C_REJECT]      = "reject",
    [PROTO_S2C_STATE]       = "state",
    [PROTO_S2C_GAMEOVER]    = "gameover",
    [PROTO_S2C_OPPONENT]    = "opponent",
    [PROTO_S2C_CHAT]        = "chat",
    [PROTO_S2C_PONG]        = "pong",
    [PROTO_S2C_PING]        = "ping",
    [PROTO_S2C_REMATCH]     = "rematch",
    [PROTO_S2C_DRAW]        = "draw",
    [PROTO_S2C_SPECTATE]    = "spectate",
    [PROTO_S2C_AUTH]        = "auth",
};

const char *proto_msg_name(ProtoMsg m)
{
    if (m < 0 || m >= PROTO_MSG_COUNT) return "";
    return PROTO_NAMES[m];
}

ProtoMsg proto_msg_from_name(const char *name)
{
    if (!name) return PROTO_MSG_UNKNOWN;
    /* Start at 1 so an empty/unknown string maps to UNKNOWN. */
    for (int i = 1; i < PROTO_MSG_COUNT; i++)
        if (strcmp(PROTO_NAMES[i], name) == 0) return (ProtoMsg)i;
    return PROTO_MSG_UNKNOWN;
}

/* Shared wire names map to their C2S enum above; re-map for the S2C side. */
static ProtoMsg to_s2c(ProtoMsg m)
{
    switch (m) {
        case PROTO_C2S_MOVE:   return PROTO_S2C_MOVE;
        case PROTO_C2S_CHAT:   return PROTO_S2C_CHAT;
        case PROTO_C2S_PING:   return PROTO_S2C_PING;
        case PROTO_C2S_PONG:   return PROTO_S2C_PONG;
        case PROTO_C2S_REMATCH:return PROTO_S2C_REMATCH;
        case PROTO_C2S_SPECTATE:return PROTO_S2C_SPECTATE;
        default: return m;
    }
}

bool proto_parse_dir(const char *text, ProtoDir dir, ProtoFrame *out)
{
    if (!text || !out) return false;
    out->type = PROTO_MSG_UNKNOWN;
    out->root = NULL;

    cJSON *root = cJSON_Parse(text);
    if (!root) return false;

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || !type->valuestring) {
        cJSON_Delete(root);
        return false;
    }
    ProtoMsg m = proto_msg_from_name(type->valuestring);
    if (m == PROTO_MSG_UNKNOWN) {
        cJSON_Delete(root);
        return false;
    }
    if (dir == PROTO_DIR_S2C) m = to_s2c(m);
    out->type = m;
    out->root = root;
    return true;
}

bool proto_parse(const char *text, ProtoFrame *out)
{
    return proto_parse_dir(text, PROTO_DIR_ANY, out);
}

void proto_frame_free(ProtoFrame *f)
{
    if (!f) return;
    if (f->root) cJSON_Delete(f->root);
    f->root = NULL;
    f->type = PROTO_MSG_UNKNOWN;
}

const char *proto_field_str(const ProtoFrame *f, const char *key)
{
    if (!f || !f->root) return NULL;
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(f->root, key);
    if (cJSON_IsString(it) && it->valuestring) return it->valuestring;
    return NULL;
}

int proto_field_int(const ProtoFrame *f, const char *key, int def)
{
    if (!f || !f->root) return def;
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(f->root, key);
    if (cJSON_IsNumber(it)) return (int)it->valuedouble;
    return def;
}

bool proto_field_bool(const ProtoFrame *f, const char *key, bool def)
{
    if (!f || !f->root) return def;
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(f->root, key);
    if (cJSON_IsBool(it)) return cJSON_IsTrue(it);
    return def;
}

cJSON *proto_new(ProtoMsg type)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    if (!cJSON_AddStringToObject(root, "type", proto_msg_name(type))) {
        cJSON_Delete(root);
        return NULL;
    }
    if (type == PROTO_C2S_HELLO)
        cJSON_AddNumberToObject(root, "v", PROTO_VERSION);
    return root;
}

char *proto_serialize(cJSON *msg)
{
    if (!msg) return NULL;
    char *text = cJSON_PrintUnformatted(msg);
    cJSON_Delete(msg);
    return text;
}
