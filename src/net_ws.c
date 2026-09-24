#include "net.h"

/*
 * Online WebSocket transport (libwebsockets). A dedicated thread owns the lws
 * context and services it; the SDL thread only enqueues outgoing lines and
 * dequeues incoming ones, so the game loop never blocks on the network.
 *
 * Without HAVE_WS these degrade to "unavailable" so the online menu entries
 * grey out, exactly like Local Multiplayer without SDL2_net.
 */

#if defined(HAVE_WS) && HAVE_WS

#include <libwebsockets.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#define WS_DBG(...) do { if (getenv("OPENCHESS_WS_VERBOSE")) fprintf(stderr, __VA_ARGS__); } while (0)

#define WS_QLEN    64
#define WS_LINELEN 1024

typedef struct {
    Transport base;
    pthread_t thread;
    pthread_mutex_t mu;
    struct lws_context *ctx;
    struct lws *wsi;
    volatile int quit;
    NetState state;
    NetRole  role;
    char host[256];
    int  port;
    int  ssl;
    char path[128];
    char inq[WS_QLEN][WS_LINELEN];
    int  in_head, in_tail, in_count;
    char outq[WS_QLEN][WS_LINELEN];
    int  out_head, out_tail, out_count;
} WsTransport;

static int ws_cb(struct lws *wsi, enum lws_callback_reasons reason,
                 void *user, void *in, size_t len);

static const struct lws_protocols WS_PROTOCOLS[] = {
    { .name = "chess", .callback = ws_cb, .rx_buffer_size = 2048 },
    { 0 }
};

static void ws_set_state(WsTransport *w, NetState s)
{
    pthread_mutex_lock(&w->mu);
    w->state = s;
    pthread_mutex_unlock(&w->mu);
}

static void *ws_thread(void *arg)
{
    WsTransport *w = arg;

    if (!getenv("OPENCHESS_WS_VERBOSE"))
        lws_set_log_level(LLL_ERR | LLL_WARN, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = WS_PROTOCOLS;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.gid = -1;
    info.uid = -1;
    info.user = w;

    struct lws_context *ctx = lws_create_context(&info);
    if (!ctx) { ws_set_state(w, NET_STATE_CLOSED); return NULL; }
    pthread_mutex_lock(&w->mu);
    w->ctx = ctx;
    pthread_mutex_unlock(&w->mu);

    struct lws_client_connect_info cc;
    memset(&cc, 0, sizeof cc);
    cc.context = ctx;
    cc.address = w->host;
    cc.port = w->port;
    cc.path = w->path;
    cc.host = w->host;
    cc.origin = w->host;
    cc.protocol = "chess";
    cc.ssl_connection = w->ssl ? LCCSCF_USE_SSL : 0;
    cc.ietf_version_or_minus_one = -1;

    if (!lws_client_connect_via_info(&cc))
        ws_set_state(w, NET_STATE_CLOSED);

    /* lws_service()'s timeout can block in poll() on some builds; drive it
     * non-blocking and pace the loop ourselves so reads/writes stay prompt. */
    while (!w->quit) {
        lws_service(ctx, 0);

        /* Kick the service thread to flush anything send() queued. */
        pthread_mutex_lock(&w->mu);
        int pending = w->out_count > 0;
        int connected = (w->state == NET_STATE_CONNECTED);
        struct lws *wsi = w->wsi;
        pthread_mutex_unlock(&w->mu);
        if (pending && connected && wsi)
            lws_callback_on_writable(wsi);

        usleep(2000);               /* 500 Hz */
    }

    WS_DBG("[ws] thread exit\n");
    lws_context_destroy(ctx);
    pthread_mutex_lock(&w->mu);
    w->ctx = NULL;
    pthread_mutex_unlock(&w->mu);
    return NULL;
}

static int ws_cb(struct lws *wsi, enum lws_callback_reasons reason,
                 void *user, void *in, size_t len)
{
    (void)user;
    WsTransport *w = (WsTransport *)lws_context_user(lws_get_context(wsi));
    if (!w) return 0;

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        pthread_mutex_lock(&w->mu);
        w->wsi = wsi;
        w->state = NET_STATE_CONNECTED;
        pthread_mutex_unlock(&w->mu);
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        WS_DBG("[ws] connection error\n");
        pthread_mutex_lock(&w->mu);
        w->wsi = NULL;
        w->state = NET_STATE_CLOSED;
        pthread_mutex_unlock(&w->mu);
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        pthread_mutex_lock(&w->mu);
        if (w->in_count < WS_QLEN) {
            size_t n = len < WS_LINELEN - 1 ? len : WS_LINELEN - 1;
            memcpy(w->inq[w->in_tail], in, n);
            w->inq[w->in_tail][n] = '\0';
            w->in_tail = (w->in_tail + 1) % WS_QLEN;
            w->in_count++;
        }
        pthread_mutex_unlock(&w->mu);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        for (;;) {
            pthread_mutex_lock(&w->mu);
            if (w->out_count == 0) { pthread_mutex_unlock(&w->mu); break; }
            char msg[WS_LINELEN];
            memcpy(msg, w->outq[w->out_head], sizeof msg);
            pthread_mutex_unlock(&w->mu);

            size_t n = strlen(msg);
            unsigned char *buf = malloc(LWS_PRE + n);
            if (!buf) break;
            memcpy(buf + LWS_PRE, msg, n);
            int m = lws_write(wsi, buf + LWS_PRE, n, LWS_WRITE_TEXT);
            free(buf);

            pthread_mutex_lock(&w->mu);
            if (m < 0) {
                w->wsi = NULL;
                w->state = NET_STATE_CLOSED;
                pthread_mutex_unlock(&w->mu);
                break;
            }
            w->out_head = (w->out_head + 1) % WS_QLEN;
            w->out_count--;
            int more = w->out_count > 0;
            pthread_mutex_unlock(&w->mu);
            if (!more) break;
        }
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        WS_DBG("[ws] closed\n");
        pthread_mutex_lock(&w->mu);
        w->wsi = NULL;
        w->state = NET_STATE_CLOSED;
        pthread_mutex_unlock(&w->mu);
        break;

    default:
        break;
    }
    return 0;
}

static int ws_poll(Transport *t, char *line, size_t cap)
{
    WsTransport *w = (WsTransport *)t;
    pthread_mutex_lock(&w->mu);
    if (w->in_count > 0) {
        size_t n = strlen(w->inq[w->in_head]);
        if (n > cap - 1) n = cap - 1;
        memcpy(line, w->inq[w->in_head], n);
        line[n] = '\0';
        w->in_head = (w->in_head + 1) % WS_QLEN;
        w->in_count--;
        pthread_mutex_unlock(&w->mu);
        return 1;
    }
    int closed = (w->state == NET_STATE_CLOSED);
    pthread_mutex_unlock(&w->mu);
    return closed ? -1 : 0;
}

static bool ws_send(Transport *t, const char *line)
{
    WsTransport *w = (WsTransport *)t;
    if (!line) return false;

    pthread_mutex_lock(&w->mu);
    if (w->state == NET_STATE_CLOSED || w->out_count >= WS_QLEN) {
        pthread_mutex_unlock(&w->mu);
        return false;
    }
    size_t n = strlen(line);
    if (n >= WS_LINELEN) n = WS_LINELEN - 1;
    memcpy(w->outq[w->out_tail], line, n);
    w->outq[w->out_tail][n] = '\0';
    w->out_tail = (w->out_tail + 1) % WS_QLEN;
    w->out_count++;
    struct lws_context *ctx = w->ctx;
    pthread_mutex_unlock(&w->mu);

    if (ctx) lws_cancel_service(ctx);
    return true;
}

static void ws_close(Transport *t)
{
    WS_DBG("[ws] close()\n");
    WsTransport *w = (WsTransport *)t;
    pthread_mutex_lock(&w->mu);
    w->quit = 1;
    struct lws_context *ctx = w->ctx;
    pthread_mutex_unlock(&w->mu);

    if (ctx) lws_cancel_service(ctx);
    pthread_join(w->thread, NULL);
    pthread_mutex_destroy(&w->mu);
    free(w);
}

static NetState ws_state(const Transport *t)
{
    WsTransport *w = (WsTransport *)t;
    pthread_mutex_lock(&w->mu);
    NetState s = w->state;
    pthread_mutex_unlock(&w->mu);
    return s;
}

static NetRole ws_role(const Transport *t)
{
    return ((WsTransport *)t)->role;
}

static const TransportOps WS_OPS = {
    .poll = ws_poll, .send = ws_send, .close = ws_close,
    .state = ws_state, .role = ws_role,
};

/* Parse "ws[s]://host[:port][/path]" into a transport. */
static bool ws_parse_url(WsTransport *w, const char *url)
{
    const char *p = url;
    if (strncmp(p, "wss://", 6) == 0) { w->ssl = 1; w->port = 443; p += 6; }
    else if (strncmp(p, "ws://", 5) == 0) { w->ssl = 0; w->port = 80; p += 5; }
    else return false;

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');
    if (colon && (!slash || colon < slash)) {
        size_t hl = (size_t)(colon - p);
        if (hl >= sizeof w->host) return false;
        memcpy(w->host, p, hl);
        w->host[hl] = '\0';
        int port = atoi(colon + 1);
        if (port <= 0 || port > 65535) return false;
        w->port = port;
    } else {
        size_t hl = slash ? (size_t)(slash - p) : strlen(p);
        if (hl >= sizeof w->host) return false;
        memcpy(w->host, p, hl);
        w->host[hl] = '\0';
    }
    snprintf(w->path, sizeof w->path, "%s", slash ? slash : "/");
    return w->host[0] != '\0';
}

bool net_ws_available(void) { return true; }

Transport *net_ws_connect(const char *url)
{
    if (!url) return NULL;

    WsTransport *w = calloc(1, sizeof *w);
    if (!w) return NULL;
    w->base.ops = &WS_OPS;
    w->role = NET_ROLE_JOIN;
    w->state = NET_STATE_CONNECTING;
    pthread_mutex_init(&w->mu, NULL);

    if (!ws_parse_url(w, url)) {
        pthread_mutex_destroy(&w->mu);
        free(w);
        return NULL;
    }
    if (pthread_create(&w->thread, NULL, ws_thread, w) != 0) {
        pthread_mutex_destroy(&w->mu);
        free(w);
        return NULL;
    }
    return &w->base;
}

#else  /* !HAVE_WS */

bool net_ws_available(void) { return false; }

Transport *net_ws_connect(const char *url)
{
    (void)url;
    return NULL;
}

#endif
