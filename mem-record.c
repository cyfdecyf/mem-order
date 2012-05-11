#include "mem.h"
#include "log.h"
#include "spinlock.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    /* Version is like write counter */
    int version;
    int last_writer;
    spinlock write_lock;
} objinfo_t;

objinfo_t *objinfo;

// Last read version for each object
DEFINE_TLS_GLOBAL(int *, prev_version);
// Memory operation count, including both read and write
DEFINE_TLS_GLOBAL(int, memop_cnt);
DEFINE_TLS_GLOBAL(FILE *, read_log);
DEFINE_TLS_GLOBAL(FILE *, write_log);

void mem_init(int nthr) {
    objinfo = calloc_check(NOBJS, sizeof(*objinfo), "objinfo");
    ALLOC_TLS_GLOBAL(nthr, prev_version);
    ALLOC_TLS_GLOBAL(nthr, memop_cnt);
    ALLOC_TLS_GLOBAL(nthr, read_log);
    ALLOC_TLS_GLOBAL(nthr, write_log);
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

    TLS(prev_version) = calloc_check(NOBJS, sizeof(*TLS(prev_version)),
        "prev_version[thrid]");
    TLS(read_log) = new_log("log/rec-rd", tid);
    TLS(write_log) = new_log("log/rec-wr", tid);
}

static void take_log(FILE *log, int memop_cnt, int objid, int version) {
    fprintf(log, "%d %d %d", memop_cnt, objid, version);
}

int32_t mem_read(int32_t *addr) {
    TLS_tid();

    int cur_version;
    int32_t val;
    int objid = obj_id(addr);
    objinfo_t *info = &objinfo[objid];

    // Re-read if there's writer
    do {
        cur_version = info->version;
        val = *addr;
    } while (cur_version != info->version);

    // If version changes since last read, there must be writes to this object.
    // During replay, this read should wait the object reach to the current
    // version.
    if (cur_version != TLS(prev_version)[objid]) {
        take_log(TLS(read_log), TLS(memop_cnt), objid, cur_version);
        TLS(prev_version)[objid] = cur_version;
    }

    TLS(memop_cnt)++;
    return val;
}

void mem_write(int32_t *addr, int32_t val) {
    TLS_tid();

    int objid = obj_id(addr);
    objinfo_t *info = &objinfo[objid];

    spin_lock(&info->write_lock);
    // There's other thread writing to this object before this write.
    // Record version so we get write-write dependency.
    if (info->last_writer != tid) {
        take_log(TLS(write_log), TLS(memop_cnt), objid, info->version);
        info->last_writer = tid;
    }
    *addr = val;
    info->version++;
    TLS(prev_version)[objid] = info->version;
    spin_unlock(&info->write_lock);
}
