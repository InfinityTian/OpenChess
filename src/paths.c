#include "paths.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
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

#if defined(__APPLE__)
    uint32_t size = (uint32_t)sizeof buf;
    if (_NSGetExecutablePath(buf, &size) != 0) buf[0] = '\0';
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n >= 0) buf[n] = '\0'; else buf[0] = '\0';
#endif

    if (!buf[0]) { snprintf(s_exe_dir, sizeof s_exe_dir, "."); return s_exe_dir; }

    char real[PATHS_MAX];
    const char *use = realpath(buf, real) ? (const char *)real : buf;
    snprintf(s_exe_dir, sizeof s_exe_dir, "%s", use);

    char *slash = strrchr(s_exe_dir, '/');
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

void path_make_parent(const char *file)
{
    if (!file || !*file) return;
    char tmp[PATHS_MAX];
    snprintf(tmp, sizeof tmp, "%s", file);

    char *slash = strrchr(tmp, '/');
    if (!slash) return;          /* no directory component */
    *slash = '\0';
    if (!tmp[0]) return;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}
