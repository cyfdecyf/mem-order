#include "mem.h"
#include "log.h"
#include "spinlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// #define DEBUG
#include "debug.h"

struct objinfo {
    /* Version is like write counter */
    volatile version_t version;
    spinlock write_lock;
};

// Information of each thread's last access
struct last_objinfo {
    version_t version;
    memop_t memop;
};

struct objinfo *g_objinfo;

__thread struct last_objinfo *g_last;
__thread memop_t memop;

__thread struct {
    struct mapped_log wait_version;
    struct mapped_log wait_memop;
} logs;

#ifdef DEBUG
__thread struct mapped_log debug_access_log;

typedef struct {
    char acc;
    memop_t memop;
    objid_t objid;
    version_t version;
    uint32_t val;
} AccessEntry;

static inline void log_access(char acc, objid_t objid, version_t ver,
        memop_t memop, uint32_t val) {
    AccessEntry *ent = (AccessEntry *)next_log_entry(&debug_access_log, sizeof(*ent));
    ent->acc = acc;
    ent->memop = memop;
    ent->objid = objid;
    ent->version = ver;
    ent->val = val;
}

#endif

void mem_init(tid_t nthr, int nobjs) {
    g_nobj = nobjs;
    g_objinfo = calloc_check(g_nobj, sizeof(*g_objinfo), "g_objinfo");
}

void mem_init_thr(tid_t tid) {
    g_last = calloc_check(g_nobj, sizeof(*g_last),
            "prev_info[tid]");
    for (int i = 0; i < g_nobj; i++) {
        // First memop cnt is 0, initialize last memop to -1 so we can
        // distinguish whether there's a last read or not.
        g_last[i].memop = -1;
    }
    new_mapped_log("version", tid, &logs.wait_version);
    new_mapped_log("memop", tid, &logs.wait_memop);

#ifdef DEBUG
    new_mapped_log("debug-access", tid, &debug_access_log);
#endif
}

static inline struct wait_version *next_version_log(struct mapped_log *log) {
    return (struct wait_version *)next_log_entry(log, sizeof(struct wait_version));
}

static inline void log_wait_version(tid_t tid, version_t current_version) {
    struct wait_version *l = next_version_log(&logs.wait_version);

    l->memop = memop;
    l->version = current_version / 2;
}

static inline struct wait_memop *next_memop_log(struct mapped_log *log) {
    return (struct wait_memop *)next_log_entry(log, sizeof(struct wait_memop));
}

static inline void log_other_wait_memop(tid_t tid, objid_t objid,
        struct last_objinfo *lastobj) {
    struct wait_memop *l = next_memop_log(&logs.wait_memop);

    l->objid = objid;
    l->version = lastobj->version / 2;
    l->memop = lastobj->memop;
}

static inline void mark_log_end(tid_t tid) {
    // printf("T%d %d logent\n", (int)tid, logcnt);
    struct wait_version *l = next_version_log(&logs.wait_version);
    l->memop = -1;
    l->version = -1;

    struct wait_memop *k = next_memop_log(&logs.wait_memop);

    k->objid = -1;
    k->version = -1;
    k->memop = -1;
}

static inline void log_order(tid_t tid, objid_t objid, version_t current_version, struct last_objinfo *lastobj) {
    log_wait_version(tid, current_version);
    if (lastobj->memop >= 0)
        log_other_wait_memop(tid, objid, lastobj);
}

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#ifdef BATCH_LOG_TAKE
enum {
    OP_WRITE = 0,
    OP_READ,
};

// memacc records basic information for memory access that can be checked
// to take ordering logs.
struct memacc {
    objid_t objid; // Use negative objid to mark write.
    version_t version;
};

#define BATCH_LOG_SIZE 10

static __thread struct memacc g_memacc_q[BATCH_LOG_SIZE];
static __thread int g_memacc_idx;
#endif

#ifdef BATCH_LOG_TAKE
static inline void take_log(struct memacc *acc) {
    char is_write = acc->objid < 0;
    objid_t objid = is_write ? ~acc->objid : acc->objid;

    struct last_objinfo *last = &g_last[objid];

    if (last->version != acc->version) {
        log_order(g_tid, objid, acc->version, last);
        last->version = acc->version;
    }
    if (is_write) {
        last->version += 2;
        last->memop = ~memop;
    } else {
        last->memop = memop;
    }
    memop++;
}

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    version_t version;
    uint32_t val;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    // Protect atomic access to version and memory.
    do {
        version = info->version;
        while (unlikely(version & 1)) {
            cpu_relax();
            version = info->version;
        }
        barrier();
        val = *addr;
        barrier();
    } while (version != info->version);

    struct memacc *acc = &g_memacc_q[g_memacc_idx];
    acc->objid = objid;
    acc->version = version;

    g_memacc_idx++;
    if (g_memacc_idx == BATCH_LOG_SIZE) {
        int i;
        for (i = 0; i < BATCH_LOG_SIZE; i++) {
            take_log(&g_memacc_q[i]);
        }
        g_memacc_idx = 0;
    }
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    version_t version;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    // Protect atomic access to version and memory.
    spin_lock(&info->write_lock);
    version = info->version;
    barrier();
    info->version++;
    barrier();
    *addr = val;
    __sync_synchronize();
    info->version++;
    spin_unlock(&info->write_lock);

    struct memacc *acc = &g_memacc_q[g_memacc_idx];
    acc->objid = ~objid;
    acc->version = version;

    g_memacc_idx++;
    if (g_memacc_idx == BATCH_LOG_SIZE) {
        int i;
        for (i = 0; i < BATCH_LOG_SIZE; i++) {
            take_log(&g_memacc_q[i]);
        }
        g_memacc_idx = 0;
    }
}

#else

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    version_t version;
    uint32_t val;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    // Avoid reording version reading before version writing the previous
    // mem_write
    //
    // The following situation may cause problem (assume A.ver = B.ver = 0
    // initialy):
    //
    //       T0          T1
    //
    // 1  lock A       lock B
    // 2
    // 3  A.ver = 1    B.ver = 1
    // 4  write A      write B
    // 5  A.ver = 2    B.ver = 2
    // 6
    // 7  v0 = B.ver   v1 = A.ver # may reorder
    // 8  read B       read A     # may reorder, but after version reading
    // 9  no conflict  no conflict
    //
    // If version reading is reordered to line 2, then v0 and v1 could both be 0.
    //
    // From T0's view, it considers T0 read B -> T1 write B.
    // From T1's view, it considers T1 read A -> T0 write A.
    //
    // Note line 8's read may also be reordered to line 2, which supports the
    // above situation.
    //
    // The contradiction is caused by the program order on both threads itself
    //
    // T0 has: T0 write A -> T0 read B -> (T1 write B -> T1 read A)
    // T1 has: T1 write B -> T1 read A -> (T0 write A -> T0 read B)
    //
    // Putting barrier near another barrier could reduce the cost of barrier
    // instruction.
    //
    // In this algorithm, the key is to disallow reorder of the actual read to
    // before the write. So barrier should be added right after the actual write
    // in mem_write function.

    do {
        // First wait until there is no writer trying to update version and
        // value.
        version = info->version;
        while (unlikely(version & 1)) {
            cpu_relax();
            version = info->version;
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

    struct last_objinfo *lastobj = &g_last[objid];

    // If version changed since last read, there must be writes to this object.
    // 1. During replay, this read should wait the object reach to the current
    // version.
    // 2. The previous memop which get previous version should happen before the
    // write.
    if (lastobj->version != version) {
        log_order(tid, objid, version, lastobj);
        // Update version so that following write after read don't need logs.
        lastobj->version = version;
    }

    /*
     *DPRINTF("T%hhd R%d obj %d @%d   \t val %d\n", tid, memop, objid,
     *    version / 2, val);
     */
#ifdef DEBUG
    log_access('R', objid, version / 2, memop, val);
#endif

    // Not every read will take logs. To get precise dependency, maintain the
    // last read memop information for each object.
    lastobj->memop = memop;
    memop++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    version_t version;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    spin_lock(&info->write_lock);

    version = info->version;
    barrier();

    // Odd version means that there's writer trying to update value.
    info->version++;
    barrier();
    *addr = val;
    // This barrier disallows read to happen before the write.
    // The explicit barrier here may also make the compiler unnecessary here.
    __sync_synchronize();
    info->version++;

    spin_unlock(&info->write_lock);

    struct last_objinfo *lastobj = &g_last[objid];

    if (lastobj->version != version) {
        log_order(tid, objid, version, lastobj);
    }

    /*
     *DPRINTF("T%hhd W%d obj %d @%d->%d\t val %d\n", tid, memop,
     *    objid, version / 2, version / 2 + 1, val);
     */
#ifdef DEBUG
    log_access('W', objid, version / 2, memop, val);
#endif

    lastobj->memop = ~memop; // flip to mark last memop as write
    lastobj->version = version + 2;
    memop++;
}
#endif

void mem_finish_thr() {
    // Must be called after all writes are done. For each object, take
    // wait_memop logs.
    // This is necessary for the last read to an object that's
    // modified by other thread later. Making a final read can record the
    // last read info which otherwise would be lost. Note the final read don't
    // need to be waited by any thread as it's not executed by the program.
    for (int i = 0; i < g_nobj; i++) {
        /*DPRINTF("T%hhd last RD obj %d @%d\n", tid, i, last[i].version / 2);*/
        if (g_last[i].version != g_objinfo[i].version &&
                g_last[i].memop >= 0) {
            log_other_wait_memop(g_tid, i, &g_last[i]);
        }
    }
    mark_log_end(g_tid);
}
