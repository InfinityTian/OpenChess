#include "paths.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#endif

#define PATHS_MAX 1024

static char s_exe_dir[PATHS_MAX];
static char s_assets[PATHS_MAX];
static char s_config[PATHS_MAX];
static int  s_exe_done = 0;
static int  s_assets_done = 0;

static int is_dir(const char *p)
{
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_file(const char *p)
{
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

const char *path_exe_dir(void)
{
    if (s_exe_done) return s_exe_dir;
    s_exe_done = 1;

    char buf[PATHS_MAX];
    buf[0] = '\0';

#if defined(_WIN32)
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)sizeof buf);
    if (n == 0 || n >= sizeof buf) buf[0] = '\0';
#elif defined(__APPLE__)
    uint32_t size = (uint32_t)sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) buf[0] = '\0';
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n >= 0) buf[n] = '\0'; else buf[0] = '\0';
#endif

    if (!buf[0]) { snprintf(s_exe_dir, sizeof s_exe_dir, "."); return s_exe_dir; }

#if defined(_WIN32)
    snprintf(s_exe_dir, sizeof s_exe_dir, "%s", buf);
#else
    char real[PATHS_MAX];
    const char *use = realpath(buf, real) ? (const char *)real : buf;
    snprintf(s_exe_dir, sizeof s_exe_dir, "%s", use);
#endif

    char *slash = strrchr(s_exe_dir, '/');
#if defined(_WIN32)
    char *bslash = strrchr(s_exe_dir, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (slash) *slash = '\0';
    else snprintf(s_exe_dir, sizeof s_exe_dir, ".");
    return s_exe_dir;
}

const char *path_assets(void)
{
    if (s_assets_done) return s_assets;
    s_assets_done = 1;

    const char *env = getenv("OPENCHESS_ASSETS");
    if (env && *env) {
        snprintf(s_assets, sizeof s_assets, "%s", env);
        return s_assets;
    }

    const char *dir = path_exe_dir();
    char p[PATHS_MAX];

    /* $PREFIX/bin/openchess -> $PREFIX/share/openchess/assets */
    snprintf(p, sizeof p, "%s/../share/openchess/assets", dir);
    if (is_dir(p)) { snprintf(s_assets, sizeof s_assets, "%s", p); return s_assets; }

    /* OpenChess.app/Contents/MacOS/openchess -> Contents/Resources/assets */
    snprintf(p, sizeof p, "%s/../Resources/assets", dir);
    if (is_dir(p)) { snprintf(s_assets, sizeof s_assets, "%s", p); return s_assets; }

    /* development: run from the repository root */
    snprintf(s_assets, sizeof s_assets, "assets");
    return s_assets;
}

const char *path_config(void)
{
    if (s_config[0]) return s_config;

    const char *env = getenv("OPENCHESS_CONFIG");
    if (env && *env) { snprintf(s_config, sizeof s_config, "%s", env); return s_config; }

    if (is_file("chess.conf")) {
        snprintf(s_config, sizeof s_config, "chess.conf");
        return s_config;
    }

#if defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        snprintf(s_config, sizeof s_config, "%s\\openchess\\chess.conf", appdata);
        return s_config;
    }
#endif

    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(s_config, sizeof s_config, "%s/openchess/chess.conf", xdg);
    else if (home && *home)
        snprintf(s_config, sizeof s_config, "%s/.config/openchess/chess.conf", home);
    else
        snprintf(s_config, sizeof s_config, "chess.conf");
    return s_config;
}

const char *path_data_dir(void)
{
    static char dir[PATHS_MAX];
    if (dir[0]) return dir;

#if defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (appdata && *appdata) {
        snprintf(dir, sizeof dir, "%s\\openchess", appdata);
        return dir;
    }
#endif

    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(dir, sizeof dir, "%s/openchess", xdg);
    else if (home && *home)
        snprintf(dir, sizeof dir, "%s/.local/share/openchess", home);
    else
        snprintf(dir, sizeof dir, ".");
    return dir;
}

const char *path_games_dir(void)
{
    static char dir[PATHS_MAX];
    if (dir[0]) return dir;
    snprintf(dir, sizeof dir, "%s/games", path_data_dir());
    return dir;
}

const char *path_puzzles_file(void)
{
    static char file[PATHS_MAX];
    if (file[0]) return file;
    snprintf(file, sizeof file, "%s/puzzles/puzzles.jsonl", path_data_dir());
    return file;
}

void path_game_file(char *out, size_t n, const char *name)
{
    if (!out || n == 0) return;

    char clean[256];
    size_t k = 0;
    for (const char *p = name; p && *p && k + 1 < sizeof clean; p++) {
        unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.')
            clean[k++] = (char)c;
        else
            clean[k++] = '_';
    }
    clean[k] = '\0';

    /* trim leading dots/spaces and a trailing ".pgn" (case-insensitive) */
    char *s = clean;
    while (*s == '.' || *s == ' ') s++;
    if (*s == '\0') snprintf(clean, sizeof clean, "game");
    else if (s != clean) memmove(clean, s, strlen(s) + 1);

    size_t len = strlen(clean);
    if (len < 4 || strcasecmp(clean + len - 4, ".pgn") != 0) {
        if (len + 4 < sizeof clean) strcat(clean, ".pgn");
    }

    snprintf(out, n, "%s/%s", path_games_dir(), clean);
}

void path_make_parent(const char *file)
{
    if (!file || !*file) return;
    char tmp[PATHS_MAX];
    snprintf(tmp, sizeof tmp, "%s", file);

    char *slash = strrchr(tmp, '/');
#if defined(_WIN32)
    char *bslash = strrchr(tmp, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    if (!slash) return;          /* no directory component */
    *slash = '\0';
    if (!tmp[0]) return;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            mkdir(tmp, 0755);
            *p = sep;
        }
    }
    mkdir(tmp, 0755);
}
