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

/* Serverless WebRTC. Exchange complete invitation / response strings manually.
 * ice may be an STUN/TURN URI, or empty for local/public-address connections. */
bool net_p2p_available(void);
bool net_is_p2p(const Transport *t);
Transport *net_p2p_create(bool host, const char *ice);
bool net_p2p_export(Transport *t, char *text, size_t cap);
bool net_p2p_import(Transport *t, const char *text);

/* Online transport. Both degrade to no-ops/false when built without HAVE_WS. */
bool net_ws_available(void);
Transport *net_ws_connect(const char *url);

#endif
