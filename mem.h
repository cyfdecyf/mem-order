#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <pthread.h>

#define BENCHMARK

#ifdef BENCHMARK
#  define NITER 500000
#  define BINARY_LOG
#else
#  define NITER 2000
#endif

// Shared object size configured to 8 to ease output
#define OBJ_SIZE 8
#define NOBJS 10

// objs are aligned to OBJ_SIZE
extern int64_t *objs;

static inline int obj_id(void *addr) {
    return ((long)addr - (long)objs) >> 3;
}

// Initialization function. Must called after nthr and thread
// data storage is initialized.
void mem_init(int nthr);
void mem_init_thr(int tid);
void mem_finish_thr();

int32_t mem_read(int tid, int32_t *addr);
void    mem_write(int tid, int32_t *addr, int32_t val);

void print_objs(void);

// Macro to handle thread local storage

// Store TLS data in a global array, use thread id to get thread local
// data.

extern pthread_key_t tid_key;
#define TLS_tid() \
    int tid = (int)(long)pthread_getspecific(tid_key)

// Defines the global array.
#define DEFINE_TLS_GLOBAL(type, var) \
    type *var##_tls
#define ALLOC_TLS_GLOBAL(nthr, var) \
    var##_tls = calloc_check(nthr, sizeof(*(var##_tls)), #var)

#define TLS_GLOBAL(var) (var##_tls)

// Use thread id to get thread local data.
// This macro must be used after the TLS_THRID macro.
#define TLS(var) (var##_tls[tid])

// Utility function

void *calloc_check(size_t nmemb, size_t size, const char *err_msg);

#define __constructor__ __attribute__((constructor))

#endif
