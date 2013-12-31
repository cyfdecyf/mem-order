#include "mem.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

/*#define DEBUG*/
#include "debug.h"

// Simluate every basic block containing RTM_BATCH_N memory accesses.
// Start RTM at the start of a basic block, and end it at the end.
static __thread int g_sim_bbcnt;

struct mapped_log g_commit_log;
tid_t g_next_thr;

void read_next_thr() {
    tid_t *ptid = (tid_t *)read_log_entry(&g_commit_log, sizeof(tid_t));
    if (!ptid) {
        fprintf(stderr, "no more threads in commit order log\n");
        /*exit(1);*/
        return;
    }
    g_next_thr = *ptid;
}

void mem_init(tid_t nthr, int nobj) {
    g_nobj = nobj;

    if (open_mapped_log_path(LOGDIR"commit", &g_commit_log) != 0) {
        DPRINTF("Can't open commit log\n");
    }
    read_next_thr();
}

void mem_init_thr(tid_t tid) {
    g_tid = tid;
}

void mem_finish_thr() {
    if (g_sim_bbcnt % RTM_BATCH_N != 0) {
        read_next_thr();
    }
}

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    int val;

    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        while (g_next_thr != g_tid) {
            sched_yield();
        }
        /*printf("T%d bb start\n", g_tid);*/
    }

    assert(g_next_thr == g_tid);
    val = *addr;

    if (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1) { // Simulate basic block end.
        /*printf("T%d bb end\n", g_tid);*/
        read_next_thr();
    }

    g_sim_bbcnt++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    if ((g_sim_bbcnt % RTM_BATCH_N) == 0) { // Simulate basic block begin.
        while (g_next_thr != g_tid) {
            sched_yield();
        }
        /*printf("T%d bb start\n", g_tid);*/
    }

    assert(g_next_thr == g_tid);
    *addr = val;

    if (g_sim_bbcnt % RTM_BATCH_N == RTM_BATCH_N - 1) { // Simulate basic block end.
        /*printf("T%d bb end\n", g_tid);*/
        read_next_thr();
    }
    g_sim_bbcnt++;
}

