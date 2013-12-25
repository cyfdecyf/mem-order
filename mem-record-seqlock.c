#include "mem-record.h"

// Provide atomic version & memory access by seqlock.

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
        log_order(objid, version, lastobj);
        // Update version so that following write after read don't need logs.
        lastobj->version = version;
    }

    /*
     *DPRINTF("T%hhd R%d obj %d @%d   \t val %d\n", tid, memop, objid,
     *    version, val);
     */
#ifdef DEBUG_ACCESS
    log_access('R', objid, version, val);
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
        log_order(objid, version, lastobj);
    }

    /*
     *DPRINTF("T%hhd W%d obj %d @%d->%d\t val %d\n", tid, memop,
     *    objid, version, version + 1, val);
     */
#ifdef DEBUG_ACCESS
    log_access('W', objid, version, val);
#endif

    lastobj->memop = ~memop; // flip to mark last memop as write
    lastobj->version = version + 2;
    memop++;
}

