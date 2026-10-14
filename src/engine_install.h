#ifndef ENGINE_INSTALL_H
#define ENGINE_INSTALL_H

typedef struct EngineInstall EngineInstall;
/* Launch the bundled installer without blocking the GUI. */
EngineInstall *engine_install_start(void);
/* 0 running, 1 success, -1 failure; frees nothing. */
int engine_install_poll(EngineInstall *install);
/* Cancel/reap the helper, including downloader children, and free it. */
void engine_install_free(EngineInstall *install);
#endif
