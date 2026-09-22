#include "ai.h"
#include "fen.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>
#define ai_access(p) (_access((p), 0))
#else
#define ai_access(p) (access((p), X_OK))
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

struct AiEngine {
#ifdef _WIN32
    HANDLE child_in;   /* parent -> engine stdin */
    HANDLE child_out;  /* engine stdout -> parent */
    HANDLE proc;
    DWORD  pid;
#else
    pid_t  pid;
    int    in_fd;      /* parent -> engine stdin */
    int    out_fd;     /* engine stdout -> parent (non-blocking) */
#endif
    char   buf[4096];
    size_t buf_len;
    int    movetime;
    int    depth;
    int    threads;
    int    hash;
    int    multipv;
    bool   searching;
    bool   alive;
    /* latest "info" score (side-to-move perspective) */
    int    last_cp;
    int    last_mate;
    int    last_depth;
    bool   last_mate_set;   /* the score came from a "score mate" line */
    bool   has_eval;
    /* Multi-PV lines, index = multipv - 1 */
    AiLine lines[AI_MAX_LINES];
    int    line_count;
};

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */

static void ai_lines_reset(AiEngine *ai);

static long now_ms(void)
{
#ifdef _WIN32
    /* Keep the value positive in a 32-bit long; wraps every ~24 days, which
     * is harmless for the short timeouts used here. */
    return (long)(GetTickCount64() & 0x7fffffff);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#endif
}

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
#endif
}

static void ai_send(AiEngine *ai, const char *cmd)
{
    if (!ai || !ai->alive) return;
    size_t len = strlen(cmd), off = 0;
    while (off < len) {
#ifdef _WIN32
        DWORD w = 0;
        if (!WriteFile(ai->child_in, cmd + off, (DWORD)(len - off), &w, NULL)) {
            ai->alive = false;
            return;
        }
        if (w == 0) { ai->alive = false; return; }
        off += (size_t)w;
#else
        ssize_t w = write(ai->in_fd, cmd + off, len - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            ai->alive = false;
            return;
        }
        off += (size_t)w;
#endif
    }
}

/* Bytes available on the engine's stdout, or -1 once it has gone away. */
static long ai_available(AiEngine *ai)
{
#ifdef _WIN32
    DWORD avail = 0;
    if (!PeekNamedPipe(ai->child_out, NULL, 0, NULL, &avail, NULL)) return -1;
    return (long)avail;
#else
    struct pollfd pfd = { ai->out_fd, POLLIN, 0 };
    int pr = poll(&pfd, 1, 0);
    if (pr < 0 && errno != EINTR) return -1;
    return pr > 0 ? 1 : 0;
#endif
}

/* Read whatever is available into the buffer. Returns bytes read (0 = none). */
static int ai_fill(AiEngine *ai)
{
    if (ai_available(ai) <= 0) {
#ifdef _WIN32
        DWORD code = 0;
        if (!GetExitCodeProcess(ai->proc, &code) || code != STILL_ACTIVE)
            ai->alive = false;
#else
        /* POSIX: read() returning 0 below is the EOF signal. */
#endif
        return 0;
    }

    char tmp[512];
    size_t n = 0;
#ifdef _WIN32
    DWORD r = 0;
    if (!ReadFile(ai->child_out, tmp, (DWORD)sizeof tmp, &r, NULL)) {
        ai->alive = false;
        return -1;
    }
    if (r == 0) { ai->alive = false; return -1; }
    n = (size_t)r;
#else
    ssize_t r = read(ai->out_fd, tmp, sizeof tmp);
    if (r == 0) { ai->alive = false; return -1; }
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        ai->alive = false;
        return -1;
    }
    n = (size_t)r;
#endif

    if (ai->buf_len + n > sizeof ai->buf) ai->buf_len = 0;     /* resync */
    memcpy(ai->buf + ai->buf_len, tmp, n);
    ai->buf_len += n;
    return (int)n;
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
        if (ai_available(ai) <= 0) break;
        if (ai_fill(ai) <= 0) break;
    }
    ai->buf_len = 0;
}

/* Read lines until one contains `token`, or the timeout elapses. */
static bool ai_wait_for(AiEngine *ai, const char *token, int timeout_ms)
{
    long deadline = now_ms() + timeout_ms;
    while (now_ms() < deadline && ai->alive) {
        if (ai_available(ai) > 0) ai_fill(ai);
        else sleep_ms(5);

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
    static char found[1024];

    if (configured && *configured) {
        if (ai_access(configured) == 0) {
            snprintf(found, sizeof found, "%s", configured);
            return found;
        }
    }

#ifdef _WIN32
    static const char *common[] = {
        "C:/Program Files/Stockfish/stockfish.exe",
        "C:/stockfish/stockfish.exe",
        NULL
    };
    const char *sep = ";";
    const char *exe = "stockfish.exe";
#else
    static const char *common[] = {
        "/opt/homebrew/bin/stockfish",
        "/usr/local/bin/stockfish",
        "/usr/bin/stockfish",
        NULL
    };
    const char *sep = ":";
    const char *exe = "stockfish";
#endif
    for (int i = 0; common[i]; i++) {
        if (ai_access(common[i]) == 0) {
            snprintf(found, sizeof found, "%s", common[i]);
            return found;
        }
    }

    const char *path = getenv("PATH");
    if (path) {
        char *dup = strdup(path);
        if (dup) {
            for (char *tok = strtok(dup, sep); tok; tok = strtok(NULL, sep)) {
                snprintf(found, sizeof found, "%s/%s", tok, exe);
                if (ai_access(found) == 0) {
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

    AiEngine *ai = calloc(1, sizeof *ai);
    if (!ai) return NULL;

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    HANDLE in_rd = NULL, in_wr = NULL, out_rd = NULL, out_wr = NULL;
    if (!CreatePipe(&in_rd, &in_wr, &sa, 0) ||
        !CreatePipe(&out_rd, &out_wr, &sa, 0)) {
        free(ai);
        return NULL;
    }
    SetHandleInformation(in_wr, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    memset(&pi, 0, sizeof pi);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in_rd;
    si.hStdOutput = out_wr;
    si.hStdError = out_wr;

    char cmdline[1200];
    snprintf(cmdline, sizeof cmdline, "\"%s\"", path);

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(in_rd);
    CloseHandle(out_wr);
    if (!ok) {
        CloseHandle(in_wr);
        CloseHandle(out_rd);
        free(ai);
        return NULL;
    }

    CloseHandle(pi.hThread);
    ai->proc = pi.hProcess;
    ai->pid = pi.dwProcessId;
    ai->child_in = in_wr;
    ai->child_out = out_rd;
#else
    int to_child[2], from_child[2];
    if (pipe(to_child) != 0) { free(ai); return NULL; }
    if (pipe(from_child) != 0) {
        close(to_child[0]); close(to_child[1]);
        free(ai);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        free(ai);
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
    ai->pid = pid;
    ai->in_fd = to_child[1];
    ai->out_fd = from_child[0];
    fcntl(ai->out_fd, F_SETFL, O_NONBLOCK);
    signal(SIGPIPE, SIG_IGN);
#endif

    ai->movetime = 400;
    ai->threads = 1;
    ai->hash = 16;
    ai->multipv = 1;
    ai->alive = true;

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

#ifdef _WIN32
    if (ai->child_in) { CloseHandle(ai->child_in); ai->child_in = NULL; }
    for (int i = 0; i < 50 && ai->alive; i++) {
        if (WaitForSingleObject(ai->proc, 0) == WAIT_OBJECT_0) {
            ai->alive = false;
            break;
        }
        sleep_ms(10);
    }
    if (ai->alive) {
        TerminateProcess(ai->proc, 0);
        WaitForSingleObject(ai->proc, 1000);
        ai->alive = false;
    }
    if (ai->child_out) { CloseHandle(ai->child_out); ai->child_out = NULL; }
    if (ai->proc) { CloseHandle(ai->proc); ai->proc = NULL; }
#else
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
#endif
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
    ai_lines_reset(ai);
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

void ai_set_threads(AiEngine *ai, int n)
{
    if (!ai || !ai->alive) return;
    if (n < 1) n = 1;
    ai->threads = n;
    char cmd[64];
    snprintf(cmd, sizeof cmd, "setoption name Threads value %d\n", n);
    ai_send(ai, cmd);
}

void ai_set_hash(AiEngine *ai, int mb)
{
    if (!ai || !ai->alive) return;
    if (mb < 1) mb = 1;
    ai->hash = mb;
    char cmd[64];
    snprintf(cmd, sizeof cmd, "setoption name Hash value %d\n", mb);
    ai_send(ai, cmd);
}

void ai_set_multipv(AiEngine *ai, int n)
{
    if (!ai || !ai->alive) return;
    if (n < 1) n = 1;
    if (n > AI_MAX_LINES) n = AI_MAX_LINES;
    ai->multipv = n;
    char cmd[64];
    snprintf(cmd, sizeof cmd, "setoption name MultiPV value %d\n", n);
    ai_send(ai, cmd);
}

int ai_get_lines(const AiEngine *ai, AiLine *out, int max)
{
    if (!ai || !out || max <= 0) return 0;
    int n = ai->line_count;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) out[i] = ai->lines[i];
    return n;
}

static void ai_lines_reset(AiEngine *ai)
{
    memset(ai->lines, 0, sizeof ai->lines);
    ai->line_count = 0;
}

/*
 * Make sure the engine has actually aborted any running search and processed
 * every command sent so far. Without this, a "position"/"go" issued while an
 * infinite search is still running is ignored and the engine keeps evaluating
 * the old position.
 */
static void ai_sync(AiEngine *ai)
{
    if (!ai || !ai->alive) return;
    ai_send(ai, "stop\n");
    ai_send(ai, "isready\n");
    ai_wait_for(ai, "readyok", 3000);
    ai->searching = false;
    ai_drain(ai);
}

void ai_go(AiEngine *ai, const Board *b)
{
    if (!ai || !ai->alive || !b) return;

    ai_sync(ai);   /* ensure a previous search is finished */
    ai->has_eval = false;
    ai->last_mate_set = false;
    ai->last_cp = ai->last_mate = ai->last_depth = 0;
    ai_lines_reset(ai);

    char fen[128];
    fen_generate(b, fen, sizeof fen);
    if (getenv("OPENCHESS_DEBUG_UCI"))
        fprintf(stderr, "uci: position fen %s\n", fen);

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

void ai_go_infinite(AiEngine *ai, const Board *b)
{
    if (!ai || !ai->alive || !b) return;

    ai_sync(ai);
    ai->has_eval = false;
    ai->last_mate_set = false;
    ai->last_cp = ai->last_mate = ai->last_depth = 0;
    ai_lines_reset(ai);

    char fen[128];
    fen_generate(b, fen, sizeof fen);
    if (getenv("OPENCHESS_DEBUG_UCI"))
        fprintf(stderr, "uci: position fen %s\n", fen);

    char cmd[160];
    snprintf(cmd, sizeof cmd, "position fen %s\n", fen);
    ai_send(ai, cmd);
    ai_send(ai, "go infinite\n");
    ai->searching = true;
}

bool ai_get_eval(const AiEngine *ai, int *cp, int *mate, int *depth)
{
    if (!ai || !ai->has_eval) return false;
    if (cp)    *cp = ai->last_cp;
    if (mate)  *mate = ai->last_mate;
    if (depth) *depth = ai->last_depth;
    return true;
}

bool ai_eval_has_mate(const AiEngine *ai)
{
    return ai && ai->has_eval && ai->last_mate_set;
}

void ai_stop_search(AiEngine *ai)
{
    if (!ai || !ai->alive) return;
    ai_send(ai, "stop\n");
    ai->searching = false;
}

static void parse_info(AiEngine *ai, const char *line)
{
    if (strncmp(line, "info ", 5) != 0) return;

    int  depth = 0, multipv = 1;
    bool have_score = false, has_mate = false;
    int  cp = 0, mate = 0;
    const char *pv = NULL;
    const char *p;

    if ((p = strstr(line, " depth "))) {
        int d = atoi(p + 7);
        if (d > 0) depth = d;
    }
    if ((p = strstr(line, " multipv "))) {
        int k = atoi(p + 9);
        if (k > 0) multipv = k;
    }
    if ((p = strstr(line, "score mate "))) {
        mate = atoi(p + 11);
        has_mate = true;
        have_score = true;
    } else if ((p = strstr(line, "score cp "))) {
        cp = atoi(p + 9);
        has_mate = false;
        have_score = true;
    }
    if ((p = strstr(line, " pv "))) pv = p + 4;

    /* The evaluation bar tracks line 1. */
    if (multipv == 1) {
        if (depth > 0) ai->last_depth = depth;
        if (have_score) {
            ai->last_cp = cp;
            ai->last_mate = mate;
            ai->last_mate_set = has_mate;
            ai->has_eval = true;
        }
    }

    if (multipv < 1 || multipv > AI_MAX_LINES) return;

    AiLine *L = &ai->lines[multipv - 1];
    L->multipv = multipv;
    if (depth > 0) L->depth = depth;
    if (have_score) {
        L->cp = cp;
        L->mate = mate;
        L->has_mate = has_mate;
    }
    if (pv) {
        size_t n = strlen(pv);
        if (n >= sizeof L->pv) n = sizeof L->pv - 1;
        memcpy(L->pv, pv, n);
        L->pv[n] = '\0';
        while (n > 0 && (L->pv[n - 1] == '\r' || L->pv[n - 1] == ' '))
            L->pv[--n] = '\0';
    }
    if (multipv > ai->line_count) ai->line_count = multipv;
}

bool ai_poll_bestmove(AiEngine *ai, char out_uci[8])
{
    if (!ai || !ai->alive || !out_uci) return false;

    ai_fill(ai);

    char line[512];
    while (ai_next_line(ai, line, sizeof line)) {
        if (strncmp(line, "bestmove ", 9) != 0) {
            parse_info(ai, line);
            continue;
        }
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
