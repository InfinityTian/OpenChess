#ifndef NET_H
#define NET_H

#include "transport.h"
#include <stdbool.h>

/*
 * LAN multiplayer (SDL2_net) plus the online WebSocket transport. Both return a
 * `Transport` the game loop drives through the generic transport_* calls; the
 * LAN constructors live here, the online one in net_ws.c.
 */

/* One-time process setup; returns false when SDL_net is missing/failed. */
bool net_init(void);
void net_shutdown(void);
bool net_available(void);

/* Start listening (host) or connect to a peer (join) on the LAN. NULL on
 * failure. Close the result with transport_close(). */
Transport *net_host(unsigned short port);
Transport *net_join(const char *host, unsigned short port);

/* Online transport. Both degrade to no-ops/false when built without HAVE_WS. */
bool net_ws_available(void);
Transport *net_ws_connect(const char *url);

#endif
