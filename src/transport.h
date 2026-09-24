#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Transport abstraction shared by every way OpenChess can talk to a peer: the
 * LAN TCP transport (SDL2_net, see net_tcp.c) and the online WebSocket
 * transport (libwebsockets, see net_ws.c). The GUI only ever talks to this
 * interface, so the two can be swapped without touching game code.
 */

typedef enum { NET_ROLE_NONE = 0, NET_ROLE_HOST, NET_ROLE_JOIN } NetRole;

typedef enum {
    NET_STATE_NONE = 0,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_CLOSED,
} NetState;

typedef struct Transport Transport;

/* Operation table implemented by each concrete transport. */
typedef struct TransportOps {
    /* Non-blocking: 1 + a line, 0 when idle, -1 when closed/errored. */
    int  (*poll)(Transport *t, char *line, size_t cap);
    /* Send one newline-terminated message. */
    bool (*send)(Transport *t, const char *line);
    void (*close)(Transport *t);
    NetState (*state)(const Transport *t);
    NetRole  (*role)(const Transport *t);
} TransportOps;

struct Transport { const TransportOps *ops; };

/* Dispatch through the vtable; every call is NULL-safe. */
int      transport_poll(Transport *t, char *line, size_t cap);
bool     transport_send(Transport *t, const char *line);
void     transport_close(Transport *t);
NetState transport_state(const Transport *t);
NetRole  transport_role(const Transport *t);

#endif
