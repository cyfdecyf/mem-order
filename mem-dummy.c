#include "mem.h"
#include <stdio.h>

int32_t mem_read(int32_t *addr) {
    return *addr;
}

void mem_write(int32_t *addr, int32_t val) {
    *addr = val;
}

void mem_init(int nthr) {}
void mem_init_thr(int tid) {}
void mem_finish_thr() {}
