#include "engine_install.h"
#include "paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
struct EngineInstall { HANDLE process, job; int result; };
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
struct EngineInstall { pid_t pid; int result; };
#endif

EngineInstall *engine_install_start(void)
{
    EngineInstall *i = calloc(1, sizeof *i);
    if (!i) return NULL;
    char script[2048], dest[2048];
    snprintf(dest, sizeof dest, "%s", path_stockfish());
    char *slash = strrchr(dest, '/');
    if (!slash) { free(i); return NULL; }
    *slash = 0;
#ifdef _WIN32
    snprintf(script, sizeof script, "%s/engine/install-stockfish.ps1", path_assets());
    char system[1024], powershell[1200], command[6000];
    GetSystemDirectoryA(system, sizeof system);
    snprintf(powershell, sizeof powershell, "%s\\WindowsPowerShell\\v1.0\\powershell.exe", system);
    snprintf(command, sizeof command,
             "\"%s\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"%s\" -Destination \"%s\"",
             powershell, script, dest);
    STARTUPINFOA si = {0}; si.cb = sizeof si;
    PROCESS_INFORMATION pi = {0};
    i->job = CreateJobObjectA(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!i->job || !SetInformationJobObject(i->job, JobObjectExtendedLimitInformation,
            &limits, sizeof limits) || !CreateProcessA(powershell, command, NULL, NULL,
            FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        if (i->job) CloseHandle(i->job);
        free(i); return NULL;
    }
    if (!AssignProcessToJobObject(i->job, pi.hProcess)) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); CloseHandle(i->job);
        free(i); return NULL;
    }
    i->process = pi.hProcess;
    ResumeThread(pi.hThread); CloseHandle(pi.hThread);
#else
    snprintf(script, sizeof script, "%s/engine/install-stockfish.sh", path_assets());
    i->pid = fork();
    if (i->pid < 0) { free(i); return NULL; }
    if (i->pid == 0) {
        setsid();
        execl("/bin/sh", "sh", script, dest, (char *)NULL);
        _exit(127);
    }
#endif
    return i;
}

int engine_install_poll(EngineInstall *i)
{
    if (i->result) return i->result;
#ifdef _WIN32
    if (WaitForSingleObject(i->process, 0) == WAIT_TIMEOUT) return 0;
    DWORD code = 1;
    GetExitCodeProcess(i->process, &code);
    i->result = code == 0 ? 1 : -1;
#else
    int status;
    pid_t result = waitpid(i->pid, &status, WNOHANG);
    if (result == 0 || (result < 0 && errno == EINTR)) return 0;
    i->result = result > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 1 : -1;
#endif
    return i->result;
}

void engine_install_free(EngineInstall *i)
{
    if (!i) return;
#ifdef _WIN32
    CloseHandle(i->job);
    CloseHandle(i->process);
#else
    if (!i->result) {
        kill(-i->pid, SIGTERM);
        kill(i->pid, SIGTERM);
        while (waitpid(i->pid, NULL, 0) < 0 && errno == EINTR) {}
    }
#endif
    free(i);
}
