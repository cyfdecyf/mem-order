#include "mem-record.h"

// Same with mem-record-seqlock, except batches log taking.

// memacc records basic information for memory access that can be checked
// to take ordering logs.
struct batmem_acc {
    objid_t objid; // Use negative objid to mark write.
    version_t version;
};

// Use a large and prime integer to make bugs easy to appear.
#define BATCH_LOG_SIZE 97

static __thread struct batmem_acc g_batch_q[BATCH_LOG_SIZE];
static __thread int g_batch_idx;

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

    if (g_batch_idx == BATCH_LOG_SIZE) {
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
    batch_process_log();
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

#ifdef DEBUG_ACCESS
    log_access('R', objid, version, val);
#endif

    batch_read_log(objid, version);
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

#ifdef DEBUG_ACCESS
    log_access('W', objid, version, val);
#endif

    batch_write_log(objid, version);
}

