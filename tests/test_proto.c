#include "../src/proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

int main(void)
{
    CHECK(PROTO_VERSION == 2);

    /* every known type has a non-empty wire name */
    for (int i = 1; i < PROTO_MSG_COUNT; i++)
        CHECK(proto_msg_name((ProtoMsg)i)[0] != '\0');

    CHECK(strcmp(proto_msg_name(PROTO_C2S_HELLO), "hello") == 0);
    CHECK(strcmp(proto_msg_name(PROTO_C2S_CANCEL_QUEUE), "cancel_queue") == 0);
    CHECK(strcmp(proto_msg_name(PROTO_S2C_GAMEOVER), "gameover") == 0);
    CHECK(strcmp(proto_msg_name(PROTO_S2C_PONG), "pong") == 0);

    CHECK(proto_msg_from_name("create") == PROTO_C2S_CREATE);
    CHECK(proto_msg_from_name("queue") == PROTO_C2S_QUEUE);
    CHECK(proto_msg_from_name("state") == PROTO_S2C_STATE);
    CHECK(proto_msg_from_name("move") == PROTO_C2S_MOVE);   /* first match */
    CHECK(proto_msg_from_name("nope") == PROTO_MSG_UNKNOWN);
    CHECK(proto_msg_from_name("") == PROTO_MSG_UNKNOWN);
    CHECK(proto_msg_from_name(NULL) == PROTO_MSG_UNKNOWN);

    CHECK(strcmp(proto_msg_name(PROTO_MSG_UNKNOWN), "") == 0);
    CHECK(proto_msg_name((ProtoMsg)-1)[0] == '\0');
    CHECK(proto_msg_name((ProtoMsg)PROTO_MSG_COUNT)[0] == '\0');

    /* framing: build -> serialize -> parse, direction-aware for shared names */
    cJSON *m = proto_new(PROTO_C2S_JOIN);
    cJSON_AddStringToObject(m, "code", "ABC123");
    char *txt = proto_serialize(m);
    CHECK(txt && strstr(txt, "\"code\":\"ABC123\""));
    free(txt);

    ProtoFrame fr;
    CHECK(proto_parse_dir("{\"type\":\"move\",\"uci\":\"e2e4\",\"ply\":3}",
                          PROTO_DIR_S2C, &fr));
    CHECK(fr.type == PROTO_S2C_MOVE);
    CHECK(strcmp(proto_field_str(&fr, "uci"), "e2e4") == 0);
    CHECK(proto_field_int(&fr, "ply", -1) == 3);
    proto_frame_free(&fr);

    CHECK(proto_parse_dir("{\"type\":\"move\",\"uci\":\"e2e4\"}",
                          PROTO_DIR_C2S, &fr));
    CHECK(fr.type == PROTO_C2S_MOVE);
    proto_frame_free(&fr);

    CHECK(!proto_parse("{not json", &fr));
    CHECK(!proto_parse("{\"type\":\"bogus\"}", &fr));

    if (failures == 0) {
        printf("proto msg name round-trip  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
