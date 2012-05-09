#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>

int32_t mem_read(int32_t *addr);
void mem_write(int32_t *addr, int32_t val);

#endif
