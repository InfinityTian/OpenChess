#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_SDL_NET) && HAVE_SDL_NET
#include <SDL_net.h>

#define NET_RBUF 1024

/* Concrete LAN transport. `base` must stay first so a Transport* can be cast
 * back to TcpTransport* inside the vtable callbacks. */
typedef struct {
    Transport base;
    NetRole  role;
    NetState state;
    TCPsocket server;      /* host listen socket while connecting */
    TCPsocket sock;        /* active connection */
    SDLNet_SocketSet set;
    char   rbuf[NET_RBUF];
    size_t rlen;
} TcpTransport;

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

static void tcp_close(Transport *t)
{
    TcpTransport *n = (TcpTransport *)t;
    if (n->set) SDLNet_FreeSocketSet(n->set);
    if (n->sock) SDLNet_TCP_Close(n->sock);
    if (n->server) SDLNet_TCP_Close(n->server);
    free(n);
}

static NetRole tcp_role(const Transport *t)
{
    return ((const TcpTransport *)t)->role;
}

static NetState tcp_state(const Transport *t)
{
    return ((const TcpTransport *)t)->state;
}

static bool pop_line(TcpTransport *n, char *line, size_t cap)
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

static int tcp_poll(Transport *t, char *line, size_t cap)
{
    TcpTransport *n = (TcpTransport *)t;

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

static bool tcp_send(Transport *t, const char *line)
{
    TcpTransport *n = (TcpTransport *)t;
    if (n->state != NET_STATE_CONNECTED || !n->sock || !line) return false;

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

static const TransportOps TCP_OPS = {
    .poll  = tcp_poll,
    .send  = tcp_send,
    .close = tcp_close,
    .state = tcp_state,
    .role  = tcp_role,
};

Transport *net_host(unsigned short port)
{
    if (!s_ok) return NULL;

    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, NULL, port) == -1) return NULL;
    TCPsocket srv = SDLNet_TCP_Open(&ip);
    if (!srv) return NULL;

    TcpTransport *n = calloc(1, sizeof *n);
    if (!n) { SDLNet_TCP_Close(srv); return NULL; }

    n->base.ops = &TCP_OPS;
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
    return &n->base;
}

Transport *net_join(const char *host, unsigned short port)
{
    if (!s_ok) return NULL;

    IPaddress ip;
    if (SDLNet_ResolveHost(&ip, host && *host ? host : NULL, port) == -1)
        return NULL;
    TCPsocket s = SDLNet_TCP_Open(&ip);
    if (!s) return NULL;

    TcpTransport *n = calloc(1, sizeof *n);
    if (!n) { SDLNet_TCP_Close(s); return NULL; }

    n->base.ops = &TCP_OPS;
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
    return &n->base;
}

#else  /* !HAVE_SDL_NET */

bool net_init(void) { return false; }
void net_shutdown(void) {}
bool net_available(void) { return false; }

Transport *net_host(unsigned short port) { (void)port; return NULL; }
Transport *net_join(const char *host, unsigned short port)
{
    (void)host; (void)port; return NULL;
}

#endif
