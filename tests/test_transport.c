#include "../src/transport.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/* A minimal in-memory transport used to exercise the vtable dispatch. */
static int  calls[5];
static char last_line[64];

static int  fake_poll(Transport *t, char *line, size_t cap)
{
    (void)t;
    calls[0]++;
    snprintf(line, cap, "PONG");
    return 1;
}
static bool fake_send(Transport *t, const char *line)
{
    (void)t;
    calls[1]++;
    snprintf(last_line, sizeof last_line, "%s", line);
    return true;
}
static void fake_close(Transport *t) { (void)t; calls[2]++; }
static NetState fake_state(const Transport *t) { (void)t; calls[3]++; return NET_STATE_CONNECTED; }
static NetRole  fake_role(const Transport *t)  { (void)t; calls[4]++; return NET_ROLE_HOST; }

static const TransportOps FAKE_OPS = {
    .poll = fake_poll, .send = fake_send, .close = fake_close,
    .state = fake_state, .role = fake_role,
};

int main(void)
{
    /* NULL transport is safe on every entry point. */
    char line[16];
    CHECK(transport_poll(NULL, line, sizeof line) == -1);
    CHECK(transport_send(NULL, "x") == false);
    CHECK(transport_state(NULL) == NET_STATE_NONE);
    CHECK(transport_role(NULL) == NET_ROLE_NONE);
    transport_close(NULL);

    /* A zeroed transport (no ops) is likewise safe. */
    Transport none = {0};
    CHECK(transport_poll(&none, line, sizeof line) == -1);
    CHECK(transport_send(&none, "x") == false);
    CHECK(transport_state(&none) == NET_STATE_NONE);
    CHECK(transport_role(&none) == NET_ROLE_NONE);
    transport_close(&none);

    /* Dispatch reaches the concrete implementation. */
    Transport t = { .ops = &FAKE_OPS };
    CHECK(transport_poll(&t, line, sizeof line) == 1 && strcmp(line, "PONG") == 0);
    CHECK(transport_send(&t, "MOVE e2e4") && strcmp(last_line, "MOVE e2e4") == 0);
    CHECK(transport_state(&t) == NET_STATE_CONNECTED);
    CHECK(transport_role(&t) == NET_ROLE_HOST);
    transport_close(&t);
    CHECK(calls[0] == 1 && calls[1] == 1 && calls[2] == 1 &&
          calls[3] == 1 && calls[4] == 1);

    if (failures == 0) {
        printf("transport vtable dispatch  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
