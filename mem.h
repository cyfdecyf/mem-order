#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>

#define __constructor__ __attribute__((constructor))

// To support thread local variable
extern pthread_key_t thrid_key;
#define DEFINE_TL_THRID() \
    long thrid = (long)pthread_getspecific(thrid_key)

// Shared object size configured to 8 to ease output
#define OBJ_SIZE 8
#define NOBJS 20

extern int nthr;
// objs are aligned to OBJ_SIZE
extern int64_t *objs;

static inline long obj_id(void *addr) {
    return ((long)addr - (long)objs) >> 3;
}

// Initialization function. Must called after nthr and thread
// data storage is initialized.
void mem_init(void);
void mem_init_thr(void);

int32_t mem_read(int32_t *addr);
void    mem_write(int32_t *addr, int32_t val);

void print_objs(void);

void *calloc_check(size_t nmemb, size_t size, const char *err_msg);

#endif
