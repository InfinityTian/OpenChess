#include "../src/net.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void nap(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

int main(void)
{
    if (!net_init() || !net_available()) {
        printf("SKIP: SDL_net unavailable\n");
        return 0;
    }

    const unsigned short port = 45789;
    Transport *host = net_host(port);
    CHECK(host != NULL);
    if (!host) { net_shutdown(); return 1; }

    Transport *join = net_join("127.0.0.1", port);
    CHECK(join != NULL);
    if (!join) { transport_close(host); net_shutdown(); return 1; }

    /* wait for the host to accept the connection */
    char line[128];
    bool connected = false;
    for (int i = 0; i < 500 && !connected; i++) {
        transport_poll(host, line, sizeof line);
        if (transport_state(host) == NET_STATE_CONNECTED) connected = true;
        else nap(10);
    }
    CHECK(connected);
    CHECK(transport_state(join) == NET_STATE_CONNECTED);

    /* join -> host */
    CHECK(transport_send(join, "PING"));
    bool got = false;
    for (int i = 0; i < 500 && !got; i++) {
        int r = transport_poll(host, line, sizeof line);
        if (r == 1 && strcmp(line, "PING") == 0) got = true;
        else nap(10);
    }
    CHECK(got);

    /* host -> join */
    CHECK(transport_send(host, "MOVE e2e4"));
    got = false;
    for (int i = 0; i < 500 && !got; i++) {
        int r = transport_poll(join, line, sizeof line);
        if (r == 1 && strcmp(line, "MOVE e2e4") == 0) got = true;
        else nap(10);
    }
    CHECK(got);

    transport_close(join);
    transport_close(host);
    net_shutdown();

    if (failures == 0) {
        printf("net loopback PING / MOVE  ok\n\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d FAILURES\n", failures);
    return 1;
}
