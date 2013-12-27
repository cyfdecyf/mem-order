#include "mem-record.h"
#include "log.h"
#include "ticket-spinlock.h"
#include "tsx/rtm.h"
#include "tsx/assert.h"
#include <stdio.h>
#include <assert.h>

// Record RTM commit order. Upon abort, acquire a global lock to execute in
// sequential order.

static __thread int g_sim_bbcnt;

static __thread struct mapped_log g_commit_log;

static inline void log_commit(uint64_t ts) {
    uint64_t *p = (uint64_t *)next_log_entry(&g_commit_log, sizeof(*p));
    *p = ts;
}

static ticketlock g_lock;

void mem_init(tid_t nthr, int nobj) { }

void mem_init_thr(tid_t tid) {
    g_tid = tid;
    new_mapped_log("commit", tid, &g_commit_log);
}

void mem_finish_thr() {
    if (g_sim_bbcnt % RTM_BATCH_N != 0) {
        // Simulated basic block not finished.
        if (_xtest()) { // Inside RTM
            if (!ticket_lockable(&g_lock)) {
                _xabort(1);
            }
            uint64_t ts = rdtsc();
            _xend();
            log_commit(ts);
        } else {
            // For threads not inside RTM, it must have acquired the ticket lock
            // in order to execute. So must release it before exit.
            uint64_t ts = rdtsc();
            ticket_unlock(&g_lock);
            log_commit(ts);
        }
    }
}

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    uint32_t val;

    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        assert(!_xtest());
        int ret = 0;
        if ((ret = _xbegin()) != _XBEGIN_STARTED) {
#ifdef RTM_STAT
            fprintf(stderr, "T%d R%ld aborted %x, %d\n", g_tid, memop, ret,
                    _XABORT_CODE(ret));
            g_rtm_abort_cnt++;
#endif
            ticket_lock(&g_lock);
        }
    }

    val = *addr;

    if (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1) { // Simulate basic block end.
        if (_xtest()) { // Inside RTM.
            // Check if there are threads executing in fallback handler.
            if (!ticket_lockable(&g_lock)) {
                _xabort(1);
            }
            uint64_t ts = rdtsc();
            _xend();
            log_commit(ts);
        } else {
            uint64_t ts = rdtsc();
            ticket_unlock(&g_lock);
            log_commit(ts);
        }
    }

    g_sim_bbcnt++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        assert(!_xtest());
        int ret = 0;
        if ((ret = _xbegin()) != _XBEGIN_STARTED) {
#ifdef RTM_STAT
            fprintf(stderr, "T%d R%ld aborted %x, %d\n", g_tid, memop, ret,
                    _XABORT_CODE(ret));
            g_rtm_abort_cnt++;
#endif
            ticket_lock(&g_lock);
        }
    }

    *addr = val;

    if (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1) { // Simulate basic block end.
        if (_xtest()) { // Inside RTM.
            // Check if there are threads executing in fallback handler.
            if (!ticket_lockable(&g_lock)) {
                _xabort(1);
            }
            uint64_t ts = rdtsc();
            _xend();
            log_commit(ts);
        } else {
            uint64_t ts = rdtsc();
            ticket_unlock(&g_lock);
            log_commit(ts);
        }
    }

    g_sim_bbcnt++;
}

