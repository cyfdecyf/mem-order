#ifndef _MEM_H
#define _MEM_H

#ifdef __cplusplus
extern "C" {
#else
// In order for racey to compile.
// Only gcc requires this, g++ enables _GNU_SOURCE by default.
#define _GNU_SOURCE
#endif

#include <stdint.h>

//#define BATCH_LOG_TAKE
// Note RTM cluster can't use DEBUG_ACCESS.
//#define DEBUG_ACCESS

// Number of shared objects, must be initialized first.
extern int g_nobj;

typedef long version_t;
typedef int objid_t;
typedef long memop_t;
typedef int8_t tid_t;

// wait_memop is used when recording.
struct wait_memop {
    objid_t objid;
    version_t version;
    memop_t memop;
};

// Version log does not need preprocessing during replay.
struct wait_version {
    memop_t memop;
    version_t version;
};

#ifdef DEBUG_ACCESS
// Record memory access information.
struct mem_acc {
    char acc;
    objid_t objid;
    uint32_t val;
    version_t version;
    memop_t memop; // necessary?
};
#endif

/*extern int struct wait_memop_wrong_size[sizeof(struct wait_memop) ==
    (sizeof(objid_t) + sizeof(version_t) + sizeof(memop_t)) ? 1 : -1];*/

// Used during replay.
struct replay_wait_memop {
    // Order of field must match with binary log
    version_t version;
    memop_t memop;
    tid_t tid;
};

// Test program should provide obj_id implementation.
extern objid_t (*calc_objid)(void *addr);

// Initialization function. Must called after nthr and thread
// data storage is initialized.
#ifndef DUMMY
void mem_init(tid_t nthr, int nobj);
void mem_init_thr(tid_t tid);
void mem_finish_thr();

uint32_t mem_read(tid_t tid, uint32_t *addr);
void    mem_write(tid_t tid, uint32_t *addr, uint32_t val);
#else // DUMMY
static inline uint32_t mem_read(tid_t tid, uint32_t *addr) {
    return *addr;
}

static inline void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    *addr = val;
}

static inline void mem_init(tid_t nthr, int nobj) {}
static inline void mem_init_thr(tid_t tid) {}
static inline void mem_finish_thr() {}
#endif

// gcc on linux supports __thread. It's much pleasant to use than using
// global array and pthread_getspecific etc.
extern __thread tid_t g_tid;

// Utility function

void *calloc_check(long nmemb, long size, const char *err_msg);

#define likely(x) __builtin_expect(!!(x), 0)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define __constructor__ __attribute__((constructor))

#ifdef __cplusplus
}
#endif

#endif
