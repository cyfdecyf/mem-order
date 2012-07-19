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

__thread last_objinfo_t * last;
__thread memop_t memop;

#ifdef BINARY_LOG
__thread struct {
    MappedLog wait_version;
    MappedLog wait_memop;
} logs;
#else
__thread struct {
    FILE *wait_version;
    FILE *wait_memop;
} logs;
#endif

#ifdef DEBUG
__thread MappedLog debug_access_log;

typedef struct {
    char acc;
    memop_t memop;
    objid_t objid;
    version_t version;
    int32_t val;
} AccessEntry;

static inline void log_access(char acc, objid_t objid, version_t ver,
        memop_t memop, int32_t val) {
    AccessEntry *ent = (AccessEntry *)next_log_entry(&debug_access_log, sizeof(*ent));
    ent->acc = acc;
    ent->memop = memop;
    ent->objid = objid;
    ent->version = ver;
    ent->val = val;
}

#endif

void mem_init(tid_t nthr) {
    objinfo = calloc_check(NOBJS, sizeof(*objinfo), "objinfo");
}

void mem_init_thr(tid_t tid) {
    last = calloc_check(NOBJS, sizeof(*last),
            "prev_info[tid]");
    for (int i = 0; i < NOBJS; i++) {
        // First memop cnt is 0, initialize last memop to -1 so we can
        // distinguish whether there's a last read or not.
        last[i].memop = -1;
    }
#ifdef BINARY_LOG
    new_mapped_log("version", tid, &logs.wait_version);
    new_mapped_log("memop", tid, &logs.wait_memop);
#else
    logs.wait_version = new_log("version", tid);
    logs.wait_memop = new_log("memop", tid);
#endif

#ifdef DEBUG
    new_mapped_log("debug-access", tid, &debug_access_log);
#endif
}

#ifdef BINARY_LOG

static inline WaitVersion *next_version_log(MappedLog *log) {
    return (WaitVersion *)next_log_entry(log, sizeof(WaitVersion));
}

static inline void log_wait_version(tid_t tid, version_t current_version) {
    WaitVersion *l = next_version_log(&logs.wait_version);

    l->memop = memop;
    l->version = current_version / 2;
}

static inline WaitMemop *next_memop_log(MappedLog *log) {
    return (WaitMemop *)next_log_entry(log, sizeof(WaitMemop));
}

static inline void log_other_wait_memop(tid_t tid, objid_t objid,
        last_objinfo_t *lastobj) {
    WaitMemop *l = next_memop_log(&logs.wait_memop);

    l->objid = objid;
    l->version = lastobj->version / 2;
    l->memop = lastobj->memop;
}

static inline void mark_log_end(tid_t tid) {
    // printf("T%d %d logent\n", (int)tid, logcnt);
    WaitVersion *l = next_version_log(&logs.wait_version);
    l->memop = -1;
    l->version = -1;

    WaitMemop *k = next_memop_log(&logs.wait_memop);

    k->objid = -1;
    k->version = -1;
    k->memop = -1;
}

#else // BINARY_LOG
// Wait version is used by thread self to wait other thread.
static inline void log_wait_version(tid_t tid, version_t current_version) {
    fprintf(logs.wait_version, "%d %d\n", (int)current_version / 2, (int)memop);
}

// Wait memop is used by other thread to wait the last memory access of self.
static inline void log_other_wait_memop(tid_t tid, objid_t objid, last_objinfo_t *lastobj) {
    fprintf(logs.wait_memop, "%d %d %d\n", (int)objid, (int)(lastobj->version / 2),
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

    last_objinfo_t *lastobj = &last[objid];

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
    // This barrier disallows read to happen before the write.
    // The explicit barrier here may also make the compiler unnecessary here.
    __sync_synchronize();
    info->version++;

    spin_unlock(&info->write_lock);

    last_objinfo_t *lastobj = &last[objid];

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

    lastobj->memop = memop;
    lastobj->version = version + 2;
    memop++;
}

void mem_finish_thr() {
    // Must be called after all writes are done. For each object, take
    // wait_memop logs.
    // This is necessary for the last read to an object that's
    // modified by other thread later. Making a final read can record the
    // last read info which otherwise would be lost. Note the final read don't
    // need to be waited by any thread as it's not executed by the program.
    for (int i = 0; i < NOBJS; i++) {
        /*DPRINTF("T%hhd last RD obj %d @%d\n", tid, i, last[i].version / 2);*/
        if (last[i].version != objinfo[i].version) {
            log_other_wait_memop(tid, i, &last[i]);
        }
    }
#ifdef BINARY_LOG
    mark_log_end(tid);
#endif
}
