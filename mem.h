#ifndef _MEM_H
#define _MEM_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// #define BENCHMARK

#ifdef BENCHMARK
#  define NITER 500000
#  define BINARY_LOG
#else
#  define NITER 2000
#  define BINARY_LOG
#endif

// Shared object size configured to 8 to ease output
#define OBJ_SIZE 8
#define NOBJS 10

typedef int version_t;
typedef int8_t objid_t;
typedef int memop_t;
typedef int8_t tid_t;

// WaitMemop is used when recording
typedef struct {
    objid_t objid;
    version_t version;
    memop_t memop;
} __attribute__((packed)) WaitMemop;

// Version log does not need preprocessing during replay
typedef struct {
    memop_t memop;
    version_t version;
} __attribute__((packed)) WaitVersion;

extern int WaitMemop_wrong_size[sizeof(WaitMemop) ==
    (sizeof(objid_t) + sizeof(version_t) + sizeof(memop_t)) ? 1 : -1];

// Used during replay
typedef struct {
    // Order of field must match with binary log
    version_t version;
    memop_t memop;
    tid_t tid;
} __attribute__((packed)) ReplayWaitMemop;

// objs are aligned to OBJ_SIZE
extern int64_t *objs;

static inline objid_t obj_id(void *addr) {
    return ((long)addr - (long)objs) >> 3;
}

// Initialization function. Must called after nthr and thread
// data storage is initialized.
void mem_init(tid_t nthr);
void mem_init_thr(tid_t tid);
void mem_finish_thr();

int32_t mem_read(tid_t tid, int32_t *addr);
void    mem_write(tid_t tid, int32_t *addr, int32_t val);

void print_objs(void);

// gcc on linux supports __thread. It's much pleasant to use than using
// global array and pthread_getspecific etc.
extern __thread tid_t tid;

#ifndef __linux
// Macro to handle thread local storage

// Store TLS data in a global array, use thread id to get thread local
// data.

extern pthread_key_t tid_key;
#define TLS_tid() \
    tid_t tid = (tid_t)(long)pthread_getspecific(tid_key)

// Defines the global array.
#define DEFINE_TLS_GLOBAL(type, var) \
    type *var##_tls
#define ALLOC_TLS_GLOBAL(nthr, var) \
    var##_tls = calloc_check(nthr, sizeof(*(var##_tls)), #var)

#define TLS_GLOBAL(var) (var##_tls)

// Use thread id to get thread local data.
// This macro must be used after the TLS_THRID macro.
#define TLS(var) (var##_tls[tid])

#endif // __linux

// Utility function

void *calloc_check(size_t nmemb, size_t size, const char *err_msg);

#define __constructor__ __attribute__((constructor))

#ifdef __cplusplus
}
#endif

#endif
