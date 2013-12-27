#include "mem-record.h"
#include "tsx/rtm.h"
#include "tsx/assert.h"
#include <stdio.h>

// Provide atomic version & memory access by RTM (seqlock as fallback handler).

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    version_t version;
    uint32_t val;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    int ret = 0;
    if ((ret = _xbegin()) == _XBEGIN_STARTED) {
        version = info->version;
        // XXX TSX note: this transaction may execute and commit while writer is
        // inside fallback handler and has increased version by one. So the
        // writer's update to version and memory does not appear atomic to the
        // reader. The assertion may fire, but it will abort checking lock in
        // that case.
        /*tsx_assert((version & 1) == 0);*/
        val = *addr;
        // Check if lock is hold to avoid the problem mentioned above.
        // Do this at the end of TX to lower abort rate.
        //
        // Suppose are using spinlock, if we check lock at start of the
        // transaction, the following sequence will abort:
        //
        //     A       B
        //   lock
        //   ...
        //   ...
        //           xbegin
        //           check lock
        //   unlock
        //
        // The problem here is that unlock is a write which touches a single
        // memory address. If we move the checking code to the end, and lock
        // check happens after unlock, the transaction has chance to commit.
        /*
         *if (info->write_lock) {
         *    _xabort(1);
         *}
         */
        // XXX Actually we can check for odd version to ensure read tx does not
        // execute in parrallel with write (this is how seqlock works). If we
        // get even version, we are either before the 1st version inc or after
        // the 2 version inc. Though we may execute while the write lock is
        // taken, if the tx commits, the read tx happens atomically before or
        // after the write. This can avoid some unnecessary aborts caused by
        // lock taken by write.
        if (version & 1) {
            _xabort(1);
        }
        _xend();
    } else {
#ifdef RTM_STAT
        fprintf(stderr, "T%d R%ld aborted %x, %d\n", g_tid, memop, ret, _XABORT_CODE(ret));
        g_rtm_abort_cnt++;
#endif
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
    }

    struct last_objinfo *lastobj = &g_last[objid];

    if (lastobj->version != version) {
        log_order(objid, version, lastobj);
        // Update version so that following write after read don't need logs.
        lastobj->version = version;
    }

#ifdef DEBUG_ACCESS
    log_access('R', objid, version, val);
#endif

    lastobj->memop = memop;
    memop++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    version_t version;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    int ret = 0;
    if ((ret = _xbegin()) == _XBEGIN_STARTED) {
        version = info->version;
        // XXX TSX note: same as read transaction, this may execute and commit
        // while other writer is in fallback handler.
        /*tsx_assert((version & 1) == 0);*/
        barrier();
        *addr = val;
        __sync_synchronize();
        info->version += 2;
        // XXX For write, we must check for lock being taken since we need to
        // ensure exclusive write.
        // The problem happens if the write tx commits, while another write
        // fallback just finished version read and before the 1st version inc.
        // See the next comment.
        if (info->write_lock) {
            _xabort(2);
        }
        _xend();
    } else {
#ifdef RTM_STAT
        fprintf(stderr, "T%d W%ld aborted %x, %d\n", g_tid, memop, ret, _XABORT_CODE(ret));
        g_rtm_abort_cnt++;
#endif
        spin_lock(&info->write_lock);

        version = info->version;
        barrier();
        // XXX If write tx does not check for lock or just check for odd
        // version, may have problem here.

        // Odd version means that there's writer trying to update value.
        info->version++;
        barrier();
        *addr = val;
        // This barrier disallows read to happen before the write.
        // The explicit barrier here may also make the compiler unnecessary here.
        __sync_synchronize();
        info->version++;

        spin_unlock(&info->write_lock);
    }

    struct last_objinfo *lastobj = &g_last[objid];

    if (lastobj->version != version) {
        log_order(objid, version, lastobj);
    }

#ifdef DEBUG_ACCESS
    log_access('W', objid, version, val);
#endif

    lastobj->memop = ~memop; // flip to mark last memop as write
    lastobj->version = version + 2;
    memop++;
}

