#include "debug.h"
#include "mem.h"
#include "log.h"
#include "spinlock.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    /* Version is like write counter */
    volatile int version;
    spinlock write_lock;
} objinfo_t;

objinfo_t *objinfo;

// Information of each thread's last access
typedef struct {
    int version;
    int memop;
} last_objinfo_t;

DEFINE_TLS_GLOBAL(last_objinfo_t *, last);
DEFINE_TLS_GLOBAL(int, memop);

#ifdef BINARY_LOG
DEFINE_TLS_GLOBAL(MappedLog, wait_version_log);
DEFINE_TLS_GLOBAL(MappedLog, wait_memop_log);
#else
DEFINE_TLS_GLOBAL(FILE *, wait_version_log);
DEFINE_TLS_GLOBAL(FILE *, wait_memop_log);
#endif

void mem_init(int nthr) {
    objinfo = calloc_check(NOBJS, sizeof(*objinfo), "objinfo");
    ALLOC_TLS_GLOBAL(nthr, last);
    ALLOC_TLS_GLOBAL(nthr, memop);
#ifdef BINARY_LOG
    ALLOC_TLS_GLOBAL(nthr, wait_version_log);
    ALLOC_TLS_GLOBAL(nthr, wait_memop_log);
#else
    ALLOC_TLS_GLOBAL(nthr, wait_version_log);
    ALLOC_TLS_GLOBAL(nthr, wait_memop_log);
#endif
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

    TLS(last) = calloc_check(NOBJS, sizeof(*TLS(last)),
            "prev_info[tid]");
    for (int i = 0; i < NOBJS; i++) {
        // First memop cnt is 0, initialize last memop to -1 so we can
        // distinguish whether there's a last read or not.
        TLS(last)[i].memop = -1;
    }
#ifdef BINARY_LOG
    new_mapped_log("log/version", tid, &TLS(wait_version_log));
    new_mapped_log("log/memop", tid, &TLS(wait_memop_log));
#else
    TLS(wait_version_log) = new_log("log/version", tid);
    TLS(wait_memop_log) = new_log("log/memop", tid);
#endif
}

#ifdef BINARY_LOG

static inline int *next_version_log(MappedLog *log) {
    return (int *)next_log_entry(log, 2 * sizeof(int));
}

static inline void log_wait_version(int tid, int current_version) {
    MappedLog *log = &TLS(wait_version_log);

    int *start = next_version_log(log);

    *(start + 0) = TLS(memop);
    *(start + 1) = current_version / 2;
}

static inline int *next_memop_log(MappedLog *log) {
    return (int *)next_log_entry(log, 3 * sizeof(int));
}

static inline void log_other_wait_memop(int tid, int objid, last_objinfo_t *lastobj) {
    MappedLog *log = &TLS(wait_memop_log);

    int *start = next_memop_log(log);

    *(start + 0) = objid;
    *(start + 1) = lastobj->version / 2;
    *(start + 2) = lastobj->memop;
}

#else // BINARY_LOG
// Wait version is used by thread self to wait other thread.
static inline void log_wait_version(int tid, int current_version) {
    fprintf(TLS(wait_version_log), "%d %d\n", current_version / 2, TLS(memop));
}

// Wait memop is used by other thread to wait the last memory access of self.
static inline void log_other_wait_memop(int tid, int objid, last_objinfo_t *lastobj) {
    fprintf(TLS(wait_memop_log), "%d %d %d\n", objid, lastobj->version / 2,
        lastobj->memop);
}
#endif // BINARY_LOG

static inline void log_order(int tid, int objid, int current_version, last_objinfo_t *lastobj) {
    log_wait_version(tid, current_version);
    log_other_wait_memop(tid, objid, lastobj);
}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

int32_t mem_read(int tid, int32_t *addr) {
    int version;
    int32_t val;
    int objid = obj_id(addr);
    objinfo_t *info = &objinfo[objid];

    do {
        // First wait until there is no writer trying to update version and
        // value.
repeat:
        version = info->version;
        if (unlikely(version & 1)) {
            cpu_relax();
            goto repeat;
        }
        barrier();
        // When we reach here, the writer must either
        // - Not holding the write lock
        // - Holding the write lock
        //   - Has not started to update version
        //   - Has finished updating value and version
        // We try to read the actual value now.
        // If the version changes later, it means there's version and
        // value update, so read should retry.
        val = *addr;
        barrier();
        // Re-read if there's writer
    } while (version != info->version);

    last_objinfo_t *lastobj = &TLS(last)[objid];

    // If version changed since last read, there must be writes to this object.
    // 1. During replay, this read should wait the object reach to the current
    // version.
    // 2. The previous memop which get previous version should happen before the
    // write.
    if (lastobj->version != version) {
        log_order(tid, objid, version, lastobj);
        // Update version so that following write after read don't need log.
        lastobj->version = version;
    }

    DPRINTF("T%d R%d obj %d @%d   \t val %d\n", tid, TLS(memop), objid,
        version / 2, val);

    // Not every read will take log. To get precise dependency, maintain the
    // last read memop information for each object.
    lastobj->memop = TLS(memop);
    TLS(memop)++;
    return val;
}

void mem_write(int tid, int32_t *addr, int32_t val) {
    int version;
    int objid = obj_id(addr);
    objinfo_t *info = &objinfo[objid];

    spin_lock(&info->write_lock);

    version = info->version;
    barrier();

    // Odd version means that there's writer trying to update value.
    info->version++;
    barrier();
    *addr = val;
    barrier();
    info->version++;

    spin_unlock(&info->write_lock);

    last_objinfo_t *lastobj = &TLS(last)[objid];

    if (lastobj->version != version) {
        log_order(tid, objid, version, lastobj);
    }

    DPRINTF("T%d W%d obj %d @%d->%d\t val %d\n", tid, TLS(memop),
        objid, version / 2, version / 2 + 1, val);

    lastobj->memop = TLS(memop);
    lastobj->version = version + 2;
    TLS(memop)++;
}

void mem_finish_thr() {
    TLS_tid();

    // Must be called after all writes are done. For each object, take
    // wait_memop log.
    // This is necessary for the last read to an object that's
    // modified by other thread later. Making a final read can record the
    // last read info which otherwise would be lost. Note the final read don't
    // need to be waited by any thread as it's not executed by the program.
    for (int i = 0; i < NOBJS; i++) {
        DPRINTF("T%d last RD obj %d @%d\n", tid, i, TLS(last)[i].version / 2);
        if (TLS(last)[i].version != objinfo[i].version) {
            log_other_wait_memop(tid, i, &TLS(last)[i]);
        }
    }
#ifdef BINARY_LOG
    // Mark the end of log
    int *next = next_version_log(&TLS(wait_version_log));
    *next = -1;

    next = next_memop_log(&TLS(wait_memop_log));
    *next = -1;

    unmap_log((void *)TLS(wait_memop_log).end - LOG_BUFFER_SIZE, LOG_BUFFER_SIZE);
    unmap_log((void *)TLS(wait_version_log).end - LOG_BUFFER_SIZE, LOG_BUFFER_SIZE);
#endif
}
