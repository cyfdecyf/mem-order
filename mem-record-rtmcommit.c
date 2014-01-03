#include "mem-record.h"
#include "log.h"
#include "ticket-spinlock.h"
#include "tsx/rtm.h"
#include "tsx/assert.h"
#include <stdio.h>
#include <assert.h>

// Record RTM commit order. Upon abort, acquire a global lock to execute in
// sequential order.

static __rtm_force_inline inline uint64_t rdtsc() {
    uint64_t h, l;
    asm volatile (
            "rdtscp\n\t"
            "lfence\n\t"
            : "=d" (h), "=a" (l) : : "rcx", "memory");
    return (h << 32) | l;
}

static __thread int64_t g_sim_bbcnt;

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

static void __rtm_force_inline bb_start(char op) {
        assert(!_xtest());
        int ret = 0;
        if ((ret = _xbegin()) != _XBEGIN_STARTED) {
#ifdef RTM_STAT
            fprintf(stderr, "T%d %c%ld aborted %x, %d\n", g_tid, op, g_sim_bbcnt, ret,
                    _XABORT_CODE(ret));
            g_rtm_abort_cnt++;
#endif
            ticket_lock(&g_lock);
        }
}

static void __rtm_force_inline bb_end() {
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

void mem_finish_thr() {
    if (g_sim_bbcnt % RTM_BATCH_N != 0) {
        bb_end();
    }
#ifdef RTM_STAT
    if (g_rtm_abort_cnt > 0) {
        fprintf(stderr, "T%d RTM total abort %d\n", g_tid, g_rtm_abort_cnt);
    }
#endif
    unmap_truncate_log(&g_commit_log);
    fprintf(stderr, "T%d g_sim_bbcnt %ld\n", g_tid, g_sim_bbcnt);
}

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    uint32_t val;

    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        bb_start('R');
    }

    val = *addr;

    if (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1) { // Simulate basic block end.
        bb_end();
    }

    g_sim_bbcnt++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        bb_start('W');
    }

    *addr = val;

    if (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1) { // Simulate basic block end.
        bb_end();
    }

    g_sim_bbcnt++;
}

