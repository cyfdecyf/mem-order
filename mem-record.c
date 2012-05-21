#include "mem.h"
#include "log.h"
#include "spinlock.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    /* Version is like write counter */
    int version;
    spinlock write_lock;
} objinfo_t;

objinfo_t *objinfo;

typedef struct {
    int read_version;
    int write_version;
    int read_memop;
} last_objinfo_t;

// Last read version for each object
DEFINE_TLS_GLOBAL(last_objinfo_t *, last_info);
// Memory operation count, including both read and write
DEFINE_TLS_GLOBAL(int, read_memop);
DEFINE_TLS_GLOBAL(FILE *, read_log);
DEFINE_TLS_GLOBAL(FILE *, write_log);

void mem_init(int nthr) {
    objinfo = calloc_check(NOBJS, sizeof(*objinfo), "objinfo");
    ALLOC_TLS_GLOBAL(nthr, last_info);
    ALLOC_TLS_GLOBAL(nthr, read_memop);
    ALLOC_TLS_GLOBAL(nthr, read_log);
    ALLOC_TLS_GLOBAL(nthr, write_log);
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

    TLS(last_info) = calloc_check(NOBJS, sizeof(*TLS(last_info)),
            "prev_info[thrid]");
    for (int i = 0; i < NOBJS; i++) {
        // First memop id is 0, initialize last read memop to -1 so we can
        // distinguish whether there's a last read or not.
        TLS(last_info)[i].read_memop = -1;
    }
    TLS(read_log) = new_log("log/rec-rd", tid);
    TLS(write_log) = new_log("log/rec-wr", tid);
}

static inline void log_read(int objid, int version) {
    TLS_tid();
    fprintf(TLS(read_log), "%d %d %d %d\n", TLS(read_memop), objid, version,
            TLS(last_info)[objid].read_memop);
}

static inline void log_write(int objid, int version) {
    TLS_tid();
    fprintf(TLS(write_log), "%d %d\n", objid, version);
}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

int32_t mem_read(int32_t *addr) {
    TLS_tid();

    int version;
    int32_t val;
    int objid = obj_id(addr);
    objinfo_t *info = &objinfo[objid];

    do {
        // First wait until there is no writer trying to update version and
        // value.
repeat:
        version = info->version;
        barrier();
        if (unlikely(version & 1)) {
            cpu_relax();
            goto repeat;
        }
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

    // If version changed since last read, there must be writes to this object.
    // During replay, this read should wait the object reach to the current
    // version.
    if (TLS(last_info)[objid].read_version != version) {
        log_read(objid, version);
        TLS(last_info)[objid].read_version = version;
    }

    // Not every read will take log. To get precise dependency, maintain the
    // last read memop information for each thread.
    TLS(last_info)[objid].read_memop = TLS(read_memop);
    TLS(read_memop)++;
    return val;
}

void mem_write(int32_t *addr, int32_t val) {
    TLS_tid();

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

    if (TLS(last_info)[objid].write_version != version) {
        log_write(objid, version);
    }

    int new_version = version + 2;
    TLS(last_info)[objid].read_version = new_version;
    TLS(last_info)[objid].write_version = new_version;
}
