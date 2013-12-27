#include "mem-record.h"
#include "log.h"
#include "spinlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*#define DEBUG*/
#include "debug.h"

struct objinfo *g_objinfo;

__thread struct last_objinfo *g_last;

__thread memop_t memop;

__thread struct {
    struct mapped_log wait_version;
    struct mapped_log wait_memop;
} logs;

#ifdef DEBUG_ACCESS
__thread struct mapped_log debug_access_log;

// There's no need to check memop as it's increase upon every memory access.
// Besides, with batch log taking, memop is not always updated when calling
// this. 
void log_access(char acc, objid_t objid, version_t ver, uint32_t val) {
    struct mem_acc *ent = (struct mem_acc *)next_log_entry(&debug_access_log, sizeof(*ent));
    ent->acc = acc;
    ent->objid = objid;
    ent->val = val;
    ent->version = ver;
}
#endif

#ifdef RTM_STAT
__thread int g_rtm_abort_cnt;
#endif

void mem_init(tid_t nthr, int nobj) {
    g_nobj = nobj;
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

#ifdef DEBUG_ACCESS
    new_mapped_log("debug-access", tid, &debug_access_log);
#endif
}

static inline struct wait_version *next_version_log(struct mapped_log *log) {
    return (struct wait_version *)next_log_entry(log, sizeof(struct wait_version));
}

static inline void log_wait_version(version_t current_version) {
    struct wait_version *l = next_version_log(&logs.wait_version);

    l->memop = memop;
    l->version = current_version;
}

static inline struct wait_memop *next_memop_log(struct mapped_log *log) {
    return (struct wait_memop *)next_log_entry(log, sizeof(struct wait_memop));
}

static inline void log_other_wait_memop(objid_t objid,
        struct last_objinfo *lastobj) {
    struct wait_memop *l = next_memop_log(&logs.wait_memop);

    l->objid = objid;
    l->version = lastobj->version;
    l->memop = lastobj->memop;
}

static inline void mark_log_end() {
    // printf("T%d %d logent\n", (int)g_tid, logcnt);
    struct wait_version *l = next_version_log(&logs.wait_version);
    l->memop = -1;
    l->version = -1;

    struct wait_memop *k = next_memop_log(&logs.wait_memop);

    k->objid = -1;
    k->version = -1;
    k->memop = -1;
}

void log_order(objid_t objid, version_t current_version,
        struct last_objinfo *lastobj) {
    log_wait_version(current_version);
    if (lastobj->memop >= 0)
        log_other_wait_memop(objid, lastobj);
}

// Dummy function to execute before mem_finish_thr()
// Some implementation needs to call some function before mem_finish_thr.
extern void before_finish_thr() __attribute__((weak));
void before_finish_thr() {}

void mem_finish_thr() {
    before_finish_thr();
    // Must be called after all writes are done. For each object, take
    // wait_memop logs.
    // This is necessary for the last read to an object that's
    // modified by other thread later. Making a final read can record the
    // last read info which otherwise would be lost.
    for (int i = 0; i < g_nobj; i++) {
        /*DPRINTF("T%hhd last RD obj %d @%d\n", g_tid, i, last[i].version);*/
        if (g_last[i].memop >= 0) {
            log_other_wait_memop(i, &g_last[i]);
        }
    }
    mark_log_end();
#ifdef RTM_STAT
    if (g_rtm_abort_cnt > 0) {
        fprintf(stderr, "T%d RTM abort %d\n", g_tid, g_rtm_abort_cnt);
    }
#endif
}

