#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>

#define __constructor__ __attribute__((constructor))

// Shared object size configured to 8 to ease output
#define OBJ_SIZE 8
#define NOBJS 20

// objs are aligned to OBJ_SIZE
extern char *objs;

static inline long obj_id(char *addr) {
    return (long)(addr - objs) >> 3;
}

int32_t mem_read(int32_t *addr);
void mem_write(int32_t *addr, int32_t val);

void print_objs(void);

#endif
