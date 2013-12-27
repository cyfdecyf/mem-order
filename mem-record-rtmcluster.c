#include "mem-record.h"
#include "tsx/rtm.h"
#include "tsx/assert.h"
#include <stdio.h>
#include <assert.h>

// Provide atomic version & memory access by RTM, each transaction will protect
// multiple memory accesses.

// memacc records basic information for memory access that can be checked
// to take ordering logs.
struct batmem_acc {
    objid_t objid; // Use negative objid to mark write.
    version_t version;
};

static __thread struct batmem_acc g_batch_q[RTM_BATCH_N];
static __thread int g_batch_idx;

// Simluate every basic block containing RTM_BATCH_N memory accesses.
// Start RTM at the start of a basic block, and end it at the end.
static __thread int g_sim_bbcnt;

// TODO With RTM, we know all the log in the batch are atomically executed.
// Is there a way to utilize this property.

static void process_1log(struct batmem_acc *acc) {
    char is_write = acc->objid < 0;
    objid_t objid = is_write ? ~acc->objid : acc->objid;

    struct last_objinfo *last = &g_last[objid];

    if (last->version != acc->version) {
        log_order(objid, acc->version, last);
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

static void batch_process_log() {
    int i;
    for (i = 0; i < g_batch_idx; i++) {
        process_1log(&g_batch_q[i]);
    }
    g_batch_idx = 0;
}

static void batch_add_log(objid_t objid, version_t version) {
    struct batmem_acc *acc = &g_batch_q[g_batch_idx++];
    acc->objid = objid;
    acc->version = version;

    if (g_batch_idx == RTM_BATCH_N) {
        batch_process_log();
    }
}

static void batch_read_log(objid_t objid, version_t version) {
    batch_add_log(objid, version);
}

static void batch_write_log(objid_t objid, version_t version) {
    batch_add_log(~objid, version);
}

// Process all unhandled logs before thread exits.
void before_finish_thr() {
    if (g_sim_bbcnt % RTM_BATCH_N != 0) {
        // Simulated basic block not finished.
        // If we are inside RTM, just commit. Parallel fallback already checked
        // in read/write tx region.
        // Otherwise, the fallback handler has no locks holded, so nothing to be
        // done.
        if (_xtest()) {
            _xend(); 
        }
    }
    batch_process_log();
}

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    version_t version;
    uint32_t val;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        assert(!_xtest());
        int ret = _xbegin();
        (void)ret;
#ifdef RTM_STAT
        if (ret != _XBEGIN_STARTED) {
            fprintf(stderr, "T%d R%ld aborted %x, %d\n", g_tid, memop, ret,
                    _XABORT_CODE(ret));
            g_rtm_abort_cnt++;
        }
#endif
    }

    int in_rtm = _xtest();
    if (in_rtm) {
        version = info->version;
        // XXX It's possible the transaction commits while another write is in
        // fallback handler and has increased version by 1, thus we would get an
        // odd version here. Also refer to read tx in rtmseq.
        if (version & 1) {
            _xabort(1);
        }
        val = *addr;
    } else {
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

    if (in_rtm && (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1)) { // Simulate basic block end.
        // XXX Update: since we have checked odd version in tx region, we
        // will abort for parallel execution of read tx and write fallback.
        // So no need to check for lock here.
        // XXX A transaction is accessing different shared objects. Here we only
        // check for a single object's write lock, it's not enough. That's why
        // we need the odd version check in the transaction region.
        /*
         *if (info->write_lock) {
         *    _xabort(2);
         *}
         */
        _xend();
        // Avoid taking log inside transaction.
        batch_read_log(objid, version);
        batch_process_log();
    } else {
        batch_read_log(objid, version);
    }

    g_sim_bbcnt++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    version_t version;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) {
        assert(!_xtest());
        int ret = _xbegin();
        (void)ret;
#ifdef RTM_STAT
        if (ret != _XBEGIN_STARTED) {
            fprintf(stderr, "T%d W%ld aborted %x, %d\n", g_tid, memop, ret,
                    _XABORT_CODE(ret));
            g_rtm_abort_cnt++;
        }
#endif
    }

    int in_rtm = _xtest();
    if (in_rtm) {
        version = info->version;
        // XXX To ensure exclusion of write tx and fallback. Same as in read
        // transaction.
        if (info->write_lock) {
            _xabort(3);
        }
        barrier();
        *addr = val;
        barrier();
        info->version += 2;
        // XXX The barrier is necessary, because there are reordering inside a
        // transaction.  The reason is the same as in seqlock implementation.
        __sync_synchronize();
    } else {
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
    }

    if (in_rtm && (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1))  {
        // XXX Update: since we have checked lock in tx region, we
        // will abort for parallel execution of write tx and write fallback.
        // So no need to check for lock here.
        /*
         *if (info->write_lock) {
         *    _xabort(4);
         *}
         */
        _xend();
        // Avoid taking log inside transaction.
        batch_write_log(objid, version);
        batch_process_log();
    } else {
        batch_write_log(objid, version);
    }

    g_sim_bbcnt++;
}

