#include "sim.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

static const struct { uintptr_t base; size_t size; } regions[] = {
    { 0x40000000u, 0x30000u },
    { 0x48000000u, 0x10000u },
    { 0x50000000u, 0x10000u },
};

int mmio_init(int trap)
{
    size_t i;

    (void)trap;
    for (i = 0; i < sizeof regions / sizeof regions[0]; i++) {
        void *p = mmap((void *)regions[i].base, regions[i].size,
                       PROT_READ | PROT_WRITE,
                       MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE,
                       -1, 0);
        if (p == MAP_FAILED || p != (void *)regions[i].base) {
            fprintf(stderr, "sim: could not map peripheral memory at 0x%08llx (%s)\n",
                    (unsigned long long)regions[i].base,
                    p == MAP_FAILED ? strerror(errno) : "wrong address");
            return 0;
        }
    }
    periph_reset();
    return 1;
}

void mmio_unprotect_all(void) {}
