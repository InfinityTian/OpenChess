#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Tiny newline-delimited TCP transport for local (LAN/localhost) multiplayer,
 * built on SDL_net when it is available at compile time. When it is not, the
 * stubs report "unavailable" and every other call is a harmless no-op.
 */

typedef enum { NET_ROLE_NONE = 0, NET_ROLE_HOST, NET_ROLE_JOIN } NetRole;

typedef enum {
    NET_STATE_NONE = 0,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_CLOSED,
} NetState;

typedef struct Net Net;

/* One-time process setup; returns false when SDL_net is missing/failed. */
bool net_init(void);
void net_shutdown(void);
bool net_available(void);

/* Start listening (host) or connect to a peer (join). NULL on failure. */
Net *net_host(unsigned short port);
Net *net_join(const char *host, unsigned short port);
void net_close(Net *n);

NetRole  net_role(const Net *n);
NetState net_state(const Net *n);

/*
 * Non-blocking. Returns 1 and writes a line (without newline) to `line`, 0 when
 * nothing is ready, or -1 once the connection is closed/errored.
 */
int net_poll(Net *n, char *line, size_t cap);

/* Send one line (a newline is appended). Returns false on failure. */
bool net_send(Net *n, const char *line);

#endif
