#define _GNU_SOURCE
#include "sim_host.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <spawn.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern char **environ;

char host_sep(void) { return '/'; }

void sleep_ms(unsigned ms)
{
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

int thread_start(void *(*fn)(void *), void *arg)
{
    pthread_t t;

    if (pthread_create(&t, NULL, fn, arg) != 0) return -1;
    pthread_detach(t);
    return 0;
}

int exe_path(char *out, size_t n)
{
    char raw[PATH_MAX];
    char resolved[PATH_MAX];

#if defined(__APPLE__)
    uint32_t sz = (uint32_t)sizeof raw;
    if (_NSGetExecutablePath(raw, &sz) != 0) return -1;
#else
    ssize_t got = readlink("/proc/self/exe", raw, sizeof raw - 1);
    if (got < 0) return -1;
    raw[got] = 0;
#endif
    if (!realpath(raw, resolved)) return -1;
    if (strlen(resolved) + 1 > n) return -1;
    memcpy(out, resolved, strlen(resolved) + 1);
    return 0;
}

static int mkdir_one(const char *path)
{
    struct stat st;

    if (mkdir(path, 0777) == 0) return 0;
    if (errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
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
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) return 1;
    if (n > 256) return 256;
    return (int)n;
}

int run_process(HostProc *proc, char *const argv[])
{
    pid_t pid;
    int rc;

    proc->live = 0;
    proc->handle = 0;
    if (!argv || !argv[0]) return -1;
    rc = posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
    if (rc != 0) return -1;
    proc->handle = (intptr_t)pid;
    proc->live = 1;
    return 0;
}

int process_wait_any(HostProc *procs, int n)
{
    for (;;) {
        int any = 0;
        for (int i = 0; i < n; i++) {
            int st;
            pid_t got;
            if (!procs[i].live) continue;
            any = 1;
            do {
                got = waitpid((pid_t)procs[i].handle, &st, WNOHANG);
            } while (got < 0 && errno == EINTR);
            if (got == (pid_t)procs[i].handle) {
                procs[i].live = 0;
                procs[i].handle = 0;
                return i;
            }
        }
        if (!any) return -1;
        sleep_ms(20);
    }
}
