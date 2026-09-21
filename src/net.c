#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_SDL_NET) && HAVE_SDL_NET
#include <SDL_net.h>

#define NET_RBUF 1024

struct Net {
    NetRole  role;
    NetState state;
    TCPsocket server;      /* host listen socket while connecting */
    TCPsocket sock;        /* active connection */
    SDLNet_SocketSet set;
    char   rbuf[NET_RBUF];
    size_t rlen;
};

static bool s_inited = false;
static bool s_ok = false;

bool net_init(void)
{
    if (s_inited) return s_ok;
    s_inited = true;
    s_ok = (SDLNet_Init() == 0);
    return s_ok;
}

void net_shutdown(void)
{
    if (s_ok) SDLNet_Quit();
    s_ok = false;
    s_inited = false;
}

bool net_available(void)
{
    return s_ok;
}

Net *net_host(unsigned short port)
{
    if (!s_ok) return NULL;

    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, NULL, port) == -1) return NULL;
    TCPsocket srv = SDLNet_TCP_Open(&ip);
    if (!srv) return NULL;

    Net *n = calloc(1, sizeof *n);
    if (!n) { SDLNet_TCP_Close(srv); return NULL; }

    n->role = NET_ROLE_HOST;
    n->state = NET_STATE_CONNECTING;
    n->server = srv;
    n->set = SDLNet_AllocSocketSet(1);
    if (!n->set || SDLNet_TCP_AddSocket(n->set, srv) == -1) {
        if (n->set) SDLNet_FreeSocketSet(n->set);
        SDLNet_TCP_Close(srv);
        free(n);
        return NULL;
    }
    return n;
}

Net *net_join(const char *host, unsigned short port)
{
    if (!s_ok) return NULL;

    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, host && *host ? host : NULL, port) == -1)
        return NULL;
    TCPsocket s = SDLNet_TCP_Open(&ip);
    if (!s) return NULL;

    Net *n = calloc(1, sizeof *n);
    if (!n) { SDLNet_TCP_Close(s); return NULL; }

    n->role = NET_ROLE_JOIN;
    n->state = NET_STATE_CONNECTED;
    n->sock = s;
    n->set = SDLNet_AllocSocketSet(1);
    if (!n->set || SDLNet_TCP_AddSocket(n->set, s) == -1) {
        if (n->set) SDLNet_FreeSocketSet(n->set);
        SDLNet_TCP_Close(s);
        free(n);
        return NULL;
    }
    return n;
}

void net_close(Net *n)
{
    if (!n) return;
    if (n->set) SDLNet_FreeSocketSet(n->set);
    if (n->sock) SDLNet_TCP_Close(n->sock);
    if (n->server) SDLNet_TCP_Close(n->server);
    free(n);
}

NetRole net_role(const Net *n) { return n ? n->role : NET_ROLE_NONE; }
NetState net_state(const Net *n) { return n ? n->state : NET_STATE_NONE; }

static bool pop_line(Net *n, char *line, size_t cap)
{
    char *nl = memchr(n->rbuf, '\n', n->rlen);
    if (!nl) return false;

    size_t len = (size_t)(nl - n->rbuf);
    while (len > 0 && n->rbuf[len - 1] == '\r') len--;

    size_t copy = len < cap - 1 ? len : cap - 1;
    memcpy(line, n->rbuf, copy);
    line[copy] = '\0';

    size_t consumed = (size_t)(nl - n->rbuf) + 1;
    memmove(n->rbuf, n->rbuf + consumed, n->rlen - consumed);
    n->rlen -= consumed;
    return true;
}

int net_poll(Net *n, char *line, size_t cap)
{
    if (!n) return -1;

    /* Host: wait for the peer to connect. */
    if (n->state == NET_STATE_CONNECTING && n->server) {
        if (SDLNet_CheckSockets(n->set, 0) > 0 && SDLNet_SocketReady(n->server)) {
            TCPsocket c = SDLNet_TCP_Accept(n->server);
            if (c) {
                SDLNet_TCP_DelSocket(n->set, n->server);
                SDLNet_TCP_Close(n->server);
                n->server = NULL;
                n->sock = c;
                SDLNet_TCP_AddSocket(n->set, c);
                n->state = NET_STATE_CONNECTED;
            }
        }
        if (n->state != NET_STATE_CONNECTED) return 0;
    }

    if (n->state == NET_STATE_CLOSED) return -1;
    if (n->state != NET_STATE_CONNECTED) return 0;

    if (pop_line(n, line, cap)) return 1;

    if (SDLNet_CheckSockets(n->set, 0) > 0 && SDLNet_SocketReady(n->sock)) {
        char tmp[512];
        int r = SDLNet_TCP_Recv(n->sock, tmp, (int)sizeof tmp);
        if (r <= 0) {
            n->state = NET_STATE_CLOSED;
            return -1;
        }
        if (n->rlen + (size_t)r > NET_RBUF) n->rlen = 0;   /* resync on overflow */
        memcpy(n->rbuf + n->rlen, tmp, (size_t)r);
        n->rlen += (size_t)r;
        if (pop_line(n, line, cap)) return 1;
    }
    return 0;
}

bool net_send(Net *n, const char *line)
{
    if (!n || n->state != NET_STATE_CONNECTED || !n->sock || !line) return false;

    char buf[600];
    int k = snprintf(buf, sizeof buf, "%s\n", line);
    if (k < 0 || (size_t)k >= sizeof buf) return false;

    int len = (int)strlen(buf);
    if (SDLNet_TCP_Send(n->sock, buf, len) < len) {
        n->state = NET_STATE_CLOSED;
        return false;
    }
    return true;
}

#else  /* !HAVE_SDL_NET */

struct Net { int unused; };

bool net_init(void) { return false; }
void net_shutdown(void) {}
bool net_available(void) { return false; }

Net *net_host(unsigned short port) { (void)port; return NULL; }
Net *net_join(const char *host, unsigned short port) { (void)host; (void)port; return NULL; }
void net_close(Net *n) { (void)n; }

NetRole net_role(const Net *n) { (void)n; return NET_ROLE_NONE; }
NetState net_state(const Net *n) { (void)n; return NET_STATE_NONE; }

int net_poll(Net *n, char *line, size_t cap)
{
    (void)n; (void)line; (void)cap;
    return -1;
}

bool net_send(Net *n, const char *line) { (void)n; (void)line; return false; }

#endif
