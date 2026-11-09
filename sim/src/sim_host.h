#ifndef SIM_HOST_H
#define SIM_HOST_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    intptr_t handle;
    int      live;
} HostProc;

static inline void path_join(char *out, size_t n, const char *a, const char *b)
{
    size_t len;
    char tail;

    if (!b) b = "";
    if (!a || !a[0]) {
        snprintf(out, n, "%s", b);
        return;
    }
    len = strlen(a);
    tail = a[len - 1];
    if (tail == '/' || tail == '\\') snprintf(out, n, "%s%s", a, b);
    else snprintf(out, n, "%s/%s", a, b);
}

char host_sep(void);

static inline void path_native(char *out, size_t n, const char *path)
{
    size_t i = 0;
    char sep;

    if (!path) path = "";
    if (n == 0) return;
    sep = host_sep();
    for (; path[i] && i + 1 < n; i++) {
        char c = path[i];
        if (c == '/' || c == '\\') c = sep;
        out[i] = c;
    }
    out[i] = 0;
}

void sleep_ms(unsigned ms);
int  thread_start(void *(*fn)(void *), void *arg);
int  exe_path(char *out, size_t n);

static inline int exe_dir(char *out, size_t n)
{
    char *slash = NULL;
    char *p;

    if (exe_path(out, n) != 0) return -1;
    for (p = out; *p; p++)
        if (*p == '/' || *p == '\\') slash = p;
    if (!slash) {
        if (n < 2) return -1;
        out[0] = '.';
        out[1] = 0;
        return 0;
    }
    if (slash == out) {
        out[1] = 0;
        return 0;
    }
    *slash = 0;
    return 0;
}
int  mkdir_p(const char *path);
int  cpu_count(void);
int  run_process(HostProc *proc, char *const argv[]);
int  process_wait_any(HostProc *procs, int n);

static inline void host_inc(atomic_int *p)
{
    atomic_fetch_add_explicit(p, 1, memory_order_relaxed);
}

static inline int host_load(atomic_int *p)
{
    return atomic_load_explicit(p, memory_order_relaxed);
}

#endif /* SIM_HOST_H */
