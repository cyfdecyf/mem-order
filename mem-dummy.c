#include "mem.h"
#include <stdio.h>

int32_t mem_read(tid_t tid, int32_t *addr) {
    return *addr;
}

void mem_write(tid_t tid, int32_t *addr, int32_t val) {
    *addr = val;
}

void mem_init(tid_t nthr) {}
void mem_init_thr(tid_t tid) {}
void mem_finish_thr() {}
