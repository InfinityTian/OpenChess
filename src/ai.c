#include "ai.h"
#include "fen.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

struct AiEngine {
    pid_t  pid;
    int    in_fd;      /* parent -> engine stdin */
    int    out_fd;     /* engine stdout -> parent (non-blocking) */
    char   buf[4096];
    size_t buf_len;
    int    movetime;
    int    depth;
    bool   searching;
    bool   alive;
};

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void ai_send(AiEngine *ai, const char *cmd)
{
    if (!ai || !ai->alive) return;
    size_t len = strlen(cmd), off = 0;
    while (off < len) {
        ssize_t w = write(ai->in_fd, cmd + off, len - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            ai->alive = false;
            return;
        }
        off += (size_t)w;
    }
}

/* Read whatever is available into the buffer. Returns bytes read (0 = none). */
static int ai_fill(AiEngine *ai)
{
    char tmp[512];
    ssize_t r = read(ai->out_fd, tmp, sizeof tmp);
    if (r > 0) {
        size_t n = (size_t)r;
        if (ai->buf_len + n > sizeof ai->buf) {
            ai->buf_len = 0;            /* shouldn't happen; resync */
        }
        memcpy(ai->buf + ai->buf_len, tmp, n);
        ai->buf_len += n;
        return (int)n;
    }
    if (r == 0) { ai->alive = false; return -1; }
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    ai->alive = false;
    return -1;
}

/* Extract one complete line (without newline) from the buffer. */
static bool ai_next_line(AiEngine *ai, char *line, size_t n)
{
    char *nl = memchr(ai->buf, '\n', ai->buf_len);
    if (!nl) return false;

    size_t len = (size_t)(nl - ai->buf);
    while (len > 0 && (ai->buf[len - 1] == '\r')) len--;   /* trim CR */
    size_t copy = len < n - 1 ? len : n - 1;
    memcpy(line, ai->buf, copy);
    line[copy] = '\0';

    size_t consumed = (size_t)(nl - ai->buf) + 1;
    memmove(ai->buf, ai->buf + consumed, ai->buf_len - consumed);
    ai->buf_len -= consumed;
    return true;
}

static void ai_drain(AiEngine *ai)
{
    if (!ai) return;
    for (int i = 0; i < 64; i++) {
        struct pollfd pfd = { ai->out_fd, POLLIN, 0 };
        if (poll(&pfd, 1, 0) <= 0) break;
        if (ai_fill(ai) <= 0) break;
    }
    ai->buf_len = 0;
}

/* Read lines until one contains `token`, or the timeout elapses. */
static bool ai_wait_for(AiEngine *ai, const char *token, int timeout_ms)
{
    long deadline = now_ms() + timeout_ms;
    while (now_ms() < deadline && ai->alive) {
        struct pollfd pfd = { ai->out_fd, POLLIN, 0 };
        int pr = poll(&pfd, 1, 10);
        if (pr > 0) ai_fill(ai);

        char line[512];
        while (ai_next_line(ai, line, sizeof line))
            if (strstr(line, token)) return true;
    }
    return ai->alive && strstr(ai->buf, token) != NULL;
}

/* ------------------------------------------------------------------ */
/* discovery / lifecycle                                               */
/* ------------------------------------------------------------------ */

const char *ai_find_engine(const char *configured)
{
    static char found[PATH_MAX];

    if (configured && *configured) {
        if (access(configured, X_OK) == 0) {
            snprintf(found, sizeof found, "%s", configured);
            return found;
        }
    }

    static const char *common[] = {
        "/opt/homebrew/bin/stockfish",
        "/usr/local/bin/stockfish",
        "/usr/bin/stockfish",
        NULL
    };
    for (int i = 0; common[i]; i++) {
        if (access(common[i], X_OK) == 0) {
            snprintf(found, sizeof found, "%s", common[i]);
            return found;
        }
    }

    const char *path = getenv("PATH");
    if (path) {
        char *dup = strdup(path);
        if (dup) {
            for (char *tok = strtok(dup, ":"); tok; tok = strtok(NULL, ":")) {
                snprintf(found, sizeof found, "%s/stockfish", tok);
                if (access(found, X_OK) == 0) {
                    free(dup);
                    return found;
                }
            }
            free(dup);
        }
    }
    return NULL;
}

AiEngine *ai_start(const char *path)
{
    if (!path || !*path) return NULL;

    int to_child[2], from_child[2];
    if (pipe(to_child) != 0) return NULL;
    if (pipe(from_child) != 0) {
        close(to_child[0]); close(to_child[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        return NULL;
    }

    if (pid == 0) {
        /* child: wire pipes to stdin/stdout and exec the engine */
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        execl(path, path, (char *)NULL);
        execvp(path, (char *const[]){ (char *)path, NULL });
        _exit(127);
    }

    close(to_child[0]);
    close(from_child[1]);

    AiEngine *ai = calloc(1, sizeof *ai);
    if (!ai) {
        close(to_child[1]); close(from_child[0]);
        kill(pid, SIGKILL); waitpid(pid, NULL, 0);
        return NULL;
    }

    ai->pid = pid;
    ai->in_fd = to_child[1];
    ai->out_fd = from_child[0];
    ai->movetime = 400;
    ai->alive = true;
    fcntl(ai->out_fd, F_SETFL, O_NONBLOCK);
    signal(SIGPIPE, SIG_IGN);

    ai_send(ai, "uci\n");
    if (!ai_wait_for(ai, "uciok", 5000)) { ai_stop(ai); return NULL; }

    ai_send(ai, "setoption name Threads value 1\n");
    ai_send(ai, "setoption name Hash value 16\n");
    ai_send(ai, "isready\n");
    if (!ai_wait_for(ai, "readyok", 5000)) { ai_stop(ai); return NULL; }

    return ai;
}

void ai_stop(AiEngine *ai)
{
    if (!ai) return;

    if (ai->alive) ai_send(ai, "quit\n");
    if (ai->in_fd >= 0) { close(ai->in_fd); ai->in_fd = -1; }

    for (int i = 0; i < 50 && ai->alive; i++) {
        int status;
        pid_t r = waitpid(ai->pid, &status, WNOHANG);
        if (r == ai->pid) { ai->alive = false; break; }
        sleep_ms(10);
    }
    if (ai->alive) {
        kill(ai->pid, SIGKILL);
        waitpid(ai->pid, NULL, 0);
    }
    if (ai->out_fd >= 0) close(ai->out_fd);
    free(ai);
}

bool ai_alive(const AiEngine *ai)
{
    return ai && ai->alive;
}

void ai_new_game(AiEngine *ai)
{
    if (!ai || !ai->alive) return;
    ai_stop_search(ai);
    ai_drain(ai);
    ai_send(ai, "ucinewgame\n");
    ai_send(ai, "isready\n");
    ai_wait_for(ai, "readyok", 2000);
}

void ai_set_skill(AiEngine *ai, int level)
{
    if (!ai || !ai->alive) return;
    if (level < 0) level = 0;
    if (level > 20) level = 20;
    char cmd[64];
    snprintf(cmd, sizeof cmd, "setoption name Skill Level value %d\n", level);
    ai_send(ai, cmd);
}

void ai_set_movetime(AiEngine *ai, int ms)
{
    if (ai) ai->movetime = ms > 0 ? ms : 1;
}

void ai_set_depth(AiEngine *ai, int depth)
{
    if (ai) ai->depth = depth > 0 ? depth : 0;
}

void ai_go(AiEngine *ai, const Board *b)
{
    if (!ai || !ai->alive || !b) return;

    ai_drain(ai);   /* discard anything left over from a previous search */

    char fen[128];
    fen_generate(b, fen, sizeof fen);

    char cmd[160];
    snprintf(cmd, sizeof cmd, "position fen %s\n", fen);
    ai_send(ai, cmd);

    if (ai->depth > 0)
        snprintf(cmd, sizeof cmd, "go depth %d\n", ai->depth);
    else
        snprintf(cmd, sizeof cmd, "go movetime %d\n", ai->movetime);
    ai_send(ai, cmd);

    ai->searching = true;
}

void ai_stop_search(AiEngine *ai)
{
    if (!ai || !ai->alive || !ai->searching) return;
    ai_send(ai, "stop\n");
    ai->searching = false;
}

bool ai_poll_bestmove(AiEngine *ai, char out_uci[8])
{
    if (!ai || !ai->alive || !out_uci) return false;

    ai_fill(ai);

    char line[512];
    while (ai_next_line(ai, line, sizeof line)) {
        if (strncmp(line, "bestmove ", 9) != 0) continue;
        const char *tok = line + 9;
        size_t i = 0;
        while (tok[i] && !isspace((unsigned char)tok[i]) && i < 7) {
            out_uci[i] = tok[i];
            i++;
        }
        out_uci[i] = '\0';
        ai->searching = false;
        return i > 0;
    }
    return false;
}

bool ai_uci_to_move(const Board *b, const char *uci, Move *out)
{
    if (!b || !uci || !out || strlen(uci) < 4) return false;

    char from[3] = { uci[0], uci[1], '\0' };
    char to[3]   = { uci[2], uci[3], '\0' };
    int fs = algebraic_to_sq(from);
    int ts = algebraic_to_sq(to);
    if (fs < 0 || ts < 0) return false;

    Piece promo = WQ;   /* stored promotions are normalized to white minor */
    if (uci[4]) {
        switch (tolower((unsigned char)uci[4])) {
            case 'n': promo = WN; break;
            case 'b': promo = WB; break;
            case 'r': promo = WR; break;
            default:  promo = WQ; break;
        }
    }

    MoveList legal;
    gen_legal(b, &legal);
    for (int i = 0; i < legal.count; i++) {
        Move m = legal.moves[i];
        if (MOVE_FROM(m) != fs || MOVE_TO(m) != ts) continue;
        if (MOVE_FLAGS(m) & FLAG_PROMO) {
            if (MOVE_PROMO(m) != promo) continue;
        }
        *out = m;
        return true;
    }
    return false;
}

void move_to_uci(Move m, char out[6])
{
    char from[3], to[3];
    sq_to_algebraic(MOVE_FROM(m), from);
    sq_to_algebraic(MOVE_TO(m), to);
    out[0] = from[0];
    out[1] = from[1];
    out[2] = to[0];
    out[3] = to[1];
    out[4] = '\0';
    if (MOVE_FLAGS(m) & FLAG_PROMO) {
        char c = 'q';
        switch (MOVE_PROMO(m)) {
            case WN: c = 'n'; break;
            case WB: c = 'b'; break;
            case WR: c = 'r'; break;
            default: c = 'q'; break;
        }
        out[4] = c;
        out[5] = '\0';
    }
}
