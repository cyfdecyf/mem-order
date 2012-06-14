#include "mem.h"
#include "log.h"
#include "spinlock.h"
#include <stdio.h>
#include <stdlib.h>

// #define DEBUG
#include "debug.h"

typedef struct {
    /* Version is like write counter */
    volatile version_t version;
    spinlock write_lock;
} objinfo_t;

// Information of each thread's last access
typedef struct {
    version_t version;
    memop_t memop;
} last_objinfo_t;

objinfo_t *objinfo;

DEFINE_TLS_GLOBAL(last_objinfo_t *, last);
DEFINE_TLS_GLOBAL(memop_t, memop);

#ifdef BINARY_LOG
DEFINE_TLS_GLOBAL(MappedLog, wait_version_log);
DEFINE_TLS_GLOBAL(MappedLog, wait_memop_log);
#else
DEFINE_TLS_GLOBAL(FILE *, wait_version_log);
DEFINE_TLS_GLOBAL(FILE *, wait_memop_log);
#endif

void mem_init(tid_t nthr) {
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

void mem_init_thr(tid_t tid) {
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
    new_mapped_log("version", tid, &TLS(wait_version_log));
    new_mapped_log("memop", tid, &TLS(wait_memop_log));
#else
    TLS(wait_version_log) = new_log("version", tid);
    TLS(wait_memop_log) = new_log("memop", tid);
#endif
}

#ifdef BINARY_LOG

static inline WaitVersion *next_version_log(MappedLog *log) {
    return (WaitVersion *)next_log_entry(log, sizeof(WaitVersion));
}

static inline void log_wait_version(tid_t tid, version_t current_version) {
    WaitVersion *l = next_version_log(&TLS(wait_version_log));

    l->memop = TLS(memop);
    l->version = current_version / 2;
}

static inline WaitMemop *next_memop_log(MappedLog *log) {
    return (WaitMemop *)next_log_entry(log, sizeof(WaitMemop));
}

static inline void log_other_wait_memop(tid_t tid, objid_t objid,
        last_objinfo_t *lastobj) {
    WaitMemop *l = next_memop_log(&TLS(wait_memop_log));

    l->objid = objid;
    l->version = lastobj->version / 2;
    l->memop = lastobj->memop;
}

static inline void mark_log_end(tid_t tid) {
    // printf("T%d %d logent\n", (int)tid, logcnt);
    WaitVersion *l = next_version_log(&TLS(wait_version_log));
    l->memop = -1;
    l->version = -1;

    WaitMemop *k = next_memop_log(&TLS(wait_memop_log));

    k->objid = -1;
    k->version = -1;
    k->memop = -1;
}

#else // BINARY_LOG
// Wait version is used by thread self to wait other thread.
static inline void log_wait_version(tid_t tid, version_t current_version) {
    fprintf(TLS(wait_version_log), "%d %d\n", (int)current_version / 2, (int)TLS(memop));
}

// Wait memop is used by other thread to wait the last memory access of self.
static inline void log_other_wait_memop(tid_t tid, objid_t objid, last_objinfo_t *lastobj) {
    fprintf(TLS(wait_memop_log), "%d %d %d\n", (int)objid, (int)(lastobj->version / 2),
        lastobj->memop);
}
#endif // BINARY_LOG

static inline void log_order(tid_t tid, objid_t objid, version_t current_version, last_objinfo_t *lastobj) {
    log_wait_version(tid, current_version);
    log_other_wait_memop(tid, objid, lastobj);
}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

int32_t mem_read(tid_t tid, int32_t *addr) {
    version_t version;
    int32_t val;
    objid_t objid = obj_id(addr);
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

    DPRINTF("T%hhd R%d obj %d @%d   \t val %d\n", tid, TLS(memop), objid,
        version / 2, val);

    // Not every read will take log. To get precise dependency, maintain the
    // last read memop information for each object.
    lastobj->memop = TLS(memop);
    TLS(memop)++;
    return val;
}

void mem_write(tid_t tid, int32_t *addr, int32_t val) {
    version_t version;
    objid_t objid = obj_id(addr);
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

    DPRINTF("T%hhd W%d obj %d @%d->%d\t val %d\n", tid, TLS(memop),
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
        DPRINTF("T%hhd last RD obj %d @%d\n", tid, i, TLS(last)[i].version / 2);
        if (TLS(last)[i].version != objinfo[i].version) {
            log_other_wait_memop(tid, i, &TLS(last)[i]);
        }
    }
#ifdef BINARY_LOG
    mark_log_end(tid);
#endif
}
