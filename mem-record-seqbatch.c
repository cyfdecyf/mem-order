#include "mem-record.h"

// Same with mem-record-seqlock, except batches log taking.

enum {
    OP_WRITE = 0,
    OP_READ,
};

// memacc records basic information for memory access that can be checked
// to take ordering logs.
struct batmem_acc {
    objid_t objid; // Use negative objid to mark write.
    version_t version;
};

// Use a large value to make bugs easy to appear.
#define BATCH_LOG_SIZE 100

static __thread struct batmem_acc g_memacc_q[BATCH_LOG_SIZE];
static __thread int g_memacc_idx;

static void take_log1(struct batmem_acc *acc) {
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

static void take_log() {
    int i;
    for (i = 0; i < g_memacc_idx; i++) {
        take_log1(&g_memacc_q[i]);
    }
    g_memacc_idx = 0;
}

// Process unhandled logs.
void before_finish_thr() {
    take_log();
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

    struct batmem_acc *acc = &g_memacc_q[g_memacc_idx++];
    acc->objid = objid;
    acc->version = version;

    if (g_memacc_idx == BATCH_LOG_SIZE) {
        take_log();
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

#ifdef DEBUG_ACCESS
    log_access('W', objid, version, val);
#endif

    struct batmem_acc *acc = &g_memacc_q[g_memacc_idx++];
    acc->objid = ~objid;
    acc->version = version;

    if (g_memacc_idx == BATCH_LOG_SIZE) {
        take_log();
    }
}

