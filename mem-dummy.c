#include "mem.h"
#include ""
#include <stdio.h>

int32_t mem_read(int32_t *addr) {
    return *addr;
}

void mem_write(int32_t *addr, int32_t val) {
    *addr = val;
}
