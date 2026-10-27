#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "sim_host.h"

#include <stdlib.h>

char host_sep(void) { return '\\'; }

void sleep_ms(unsigned ms) { Sleep(ms); }

typedef struct {
    void *(*fn)(void *);
    void *arg;
} HostStart;

static DWORD WINAPI host_tramp(LPVOID raw)
{
    HostStart *s = (HostStart *)raw;
    void *(*fn)(void *) = s->fn;
    void *arg = s->arg;
    free(s);
    fn(arg);
    return 0;
}

int thread_start(void *(*fn)(void *), void *arg)
{
    HostStart *s;
    HANDLE h;

    s = (HostStart *)malloc(sizeof *s);
    if (!s) return -1;
    s->fn = fn;
    s->arg = arg;
    h = CreateThread(NULL, 0, host_tramp, s, 0, NULL);
    if (!h) {
        free(s);
        return -1;
    }
    CloseHandle(h);
    return 0;
}

int exe_path(char *out, size_t n)
{
    DWORD got;

    if (n == 0 || n > 0xFFFFFFFFu) return -1;
    got = GetModuleFileNameA(NULL, out, (DWORD)n);
    if (got == 0 || got >= (DWORD)n) return -1;
    return 0;
}

static int mkdir_one(const char *path)
{
    DWORD attr;

    if (CreateDirectoryA(path, NULL)) return 0;
    if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    return -1;
}

int mkdir_p(const char *path)
{
    char buf[512];
    size_t len;
    char *p;

    if (!path || !path[0]) return -1;
    len = strlen(path);
    if (len >= sizeof buf) return -1;
    memcpy(buf, path, len + 1);
    for (p = buf + 1; *p; p++) {
        if (*p != '/' && *p != '\\') continue;
        *p = 0;
        if (buf[0] && mkdir_one(buf) != 0) return -1;
        *p = '/';
    }
    return mkdir_one(buf);
}

int cpu_count(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors < 1) return 1;
    if (si.dwNumberOfProcessors > 256) return 256;
    return (int)si.dwNumberOfProcessors;
}

static int append_arg(char *dst, size_t n, const char *arg)
{
    size_t used = strlen(dst);
    int quote = (arg[0] == 0);
    const char *s;

    for (s = arg; *s; s++) {
        if (*s == ' ' || *s == '\t' || *s == '"') quote = 1;
    }
    if (used && used + 1 < n) dst[used++] = ' ';
    if (!quote) {
        size_t alen = strlen(arg);
        if (used + alen + 1 > n) return -1;
        memcpy(dst + used, arg, alen + 1);
        return 0;
    }
    if (used + 1 >= n) return -1;
    dst[used++] = '"';
    for (s = arg; *s; s++) {
        if (*s == '"') {
            if (used + 2 >= n) return -1;
            dst[used++] = '\\';
        }
        if (used + 1 >= n) return -1;
        dst[used++] = *s;
    }
    if (used + 2 > n) return -1;
    dst[used++] = '"';
    dst[used] = 0;
    return 0;
}

int run_process(HostProc *proc, char *const argv[])
{
    char cmd[8192];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    int i;

    proc->live = 0;
    proc->handle = 0;
    if (!argv || !argv[0]) return -1;
    cmd[0] = 0;
    for (i = 0; argv[i]; i++) {
        if (append_arg(cmd, sizeof cmd, argv[i]) != 0) return -1;
    }
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(argv[0], cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return -1;
    CloseHandle(pi.hThread);
    proc->handle = (intptr_t)pi.hProcess;
    proc->live = 1;
    return 0;
}

int process_wait_any(HostProc *procs, int n)
{
    HANDLE hs[64];
    int map[64];
    DWORD count = 0;
    DWORD w;
    int idx;

    for (int i = 0; i < n && count < 64; i++) {
        if (!procs[i].live) continue;
        hs[count] = (HANDLE)(intptr_t)procs[i].handle;
        map[count] = i;
        count++;
    }
    if (count == 0) return -1;
    w = WaitForMultipleObjects(count, hs, FALSE, INFINITE);
    if (w < WAIT_OBJECT_0 || w >= WAIT_OBJECT_0 + count) return -1;
    idx = map[w - WAIT_OBJECT_0];
    CloseHandle((HANDLE)(intptr_t)procs[idx].handle);
    procs[idx].live = 0;
    procs[idx].handle = 0;
    return idx;
}
