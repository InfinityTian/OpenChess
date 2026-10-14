#include "../src/engine_install.h"
#include "../src/paths.h"
#include "../src/ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define set_env(k,v) _putenv_s(k,v)
#define nap() Sleep(10)
#else
#include <unistd.h>
#define set_env(k,v) setenv(k,v,1)
#define nap() usleep(10000)
#endif

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void write_helper(const char *assets, int mode)
{
    char script[2048];
#ifdef _WIN32
    snprintf(script, sizeof script, "%s/engine/install-stockfish.ps1", assets);
#else
    snprintf(script, sizeof script, "%s/engine/install-stockfish.sh", assets);
#endif
    path_make_parent(script);
    FILE *f = fopen(script, "w");
    CHECK(f != NULL);
    if (!f) return;
#ifdef _WIN32
    fprintf(f, "param([string]$Destination)\n%s\nexit %d\n",
            mode == 2 ? "Start-Sleep -Seconds 30" : "", mode == 1 ? 1 : 0);
#else
    fprintf(f, "%s\nexit %d\n", mode == 2 ? "sleep 30" : "", mode == 1 ? 1 : 0);
#endif
    fclose(f);
}

int main(void)
{
    char base[1024], assets[1100], configured[1200];
#ifdef _WIN32
    char temp[MAX_PATH]; GetTempPathA(sizeof temp, temp);
    snprintf(base, sizeof base, "%soc-engine-test-%lu", temp, (unsigned long)GetCurrentProcessId());
    set_env("APPDATA", base);
#else
    snprintf(base, sizeof base, "/tmp/oc-engine-test-%ld", (long)getpid());
    set_env("XDG_DATA_HOME", base);
#endif
    snprintf(assets, sizeof assets, "%s/assets with spaces", base);
    set_env("OPENCHESS_ASSETS", assets);
    snprintf(configured, sizeof configured, "%s/configured engine", base);
    path_make_parent(configured);
    FILE *f = fopen(configured, "w"); CHECK(f != NULL);
    if (f) fclose(f);
#ifndef _WIN32
    /* Existence alone must not classify a non-executable as an engine. */
    CHECK(!ai_find_engine(configured) || strcmp(ai_find_engine(configured), configured));
    CHECK(chmod(configured, 0755) == 0);
#endif
    CHECK(ai_find_engine(configured) && !strcmp(ai_find_engine(configured), configured));
    CHECK(!ai_find_engine(base) || strcmp(ai_find_engine(base), base));
    CHECK(strstr(path_stockfish(), "engines/stockfish/stockfish") != NULL);
    path_make_parent(path_stockfish());
    f = fopen(path_stockfish(), "w"); CHECK(f != NULL);
    if (f) fclose(f);
#ifndef _WIN32
    CHECK(chmod(path_stockfish(), 0755) == 0);
#endif
    CHECK(ai_find_engine(NULL) && !strcmp(ai_find_engine(NULL), path_stockfish()));
    /* A custom configured engine continues to win over the private install. */
    CHECK(ai_find_engine(configured) && !strcmp(ai_find_engine(configured), configured));
    remove(path_stockfish());
    for (int mode = 0; mode < 3; mode++) {
        write_helper(assets, mode);
        EngineInstall *i = engine_install_start(); CHECK(i != NULL);
        if (!i) continue;
        if (mode == 2) {
            nap(); CHECK(engine_install_poll(i) == 0);
            /* Cancelling a helper must not wait for its sleeping child. */
            engine_install_free(i);
        } else {
            int result = 0;
            for (int n = 0; n < 1000 && !result; n++) { result = engine_install_poll(i); nap(); }
            CHECK(result == (mode == 0 ? 1 : -1));
            CHECK(engine_install_poll(i) == result);
            engine_install_free(i);
        }
    }
    printf("Engine discovery and installer lifecycle: %s\n", failures ? "FAILED" : "PASSED");
    return failures != 0;
}
