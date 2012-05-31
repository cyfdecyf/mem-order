#include "mem.h"
#include "log.h"
#include "spinlock.h" // Just for barrier and cpu_relax
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int *obj_version;

DEFINE_TLS_GLOBAL(int, memop);

typedef struct WaitMemopLog {
    int tid;
    int version;
    int memop;
} WaitMemopLog;

static inline int read_wait_memop_log(FILE *log, WaitMemopLog *ent, int *objid) {
    if (fscanf(log, "%d %d %d %d", objid, &ent->version, &ent->memop,
            &ent->tid) != 4) {
        return 0;
    }
    return 1;
}

typedef struct {
    int version;
    int memop;
} WaitVersionLog;
DEFINE_TLS_GLOBAL(WaitVersionLog, wait_version);
DEFINE_TLS_GLOBAL(FILE *, wait_version_log);
DEFINE_TLS_GLOBAL(char, no_more_wait_version);

static inline void next_wait_version_log() {
    TLS_tid();
    WaitVersionLog *ent = &TLS(wait_version);

    if (fscanf(TLS(wait_version_log), "%d %d", &ent->version,
            &ent->memop) != 2) {
        // fprintf(stderr, "No more wait version log for thread %d\n", tid);
        TLS(no_more_wait_version) = 1;
    }
}

typedef struct {
    WaitMemopLog *log;
    int n;
    int size;
} WaitMemop;

WaitMemop *wait_memop;
DEFINE_TLS_GLOBAL(int *, wait_memop_idx);

const int INIT_LOG_CNT = 1000;

static void load_wait_memop_log() {
    FILE *logfile = fopen("log/memop", "r");
    if (! logfile) {
        printf("Can't open log/memop\n");
        exit(1);
    }

    wait_memop = calloc_check(NOBJS, sizeof(*wait_memop), "Can't allocate wait_memop");

    int wait_memoplogsize = INIT_LOG_CNT * sizeof(wait_memop[0].log[0]);
    for (int i = 0; i < NOBJS; i++) {
        wait_memop[i].log = calloc_check(1, wait_memoplogsize, "Can't allocate wait_memop[i].log");
        wait_memop[i].size = INIT_LOG_CNT;
        wait_memop[i].n = 0;
    }

    int objid;
    WaitMemopLog ent;
    while (fscanf(logfile, "%d %d %d %d", &objid, &ent.version, &ent.memop, &ent.tid) == 4) {
        assert(objid < NOBJS);

        int n = wait_memop[objid].n;
        // Need to enlarge log array
        if (n >= wait_memop[objid].size) {
            unsigned int mem_size = wait_memop[objid].size * sizeof(wait_memop[0].log[0]) * 2;
            WaitMemopLog *new_log = realloc(wait_memop[objid].log, mem_size);
            if (! new_log) {
                printf("Can't reallocate for wait_memop[%d].log\n", objid);
                exit(1);
            }
            wait_memop[objid].log = new_log;
            wait_memop[objid].size *= 2;
        }

        wait_memop[objid].log[n] = ent;
        wait_memop[objid].n++;
    }

    for (int i = 0; i < NOBJS; ++i) {
        // size is then used as index to the last log.
        // n is then used as the index to the next unused log
        wait_memop[i].size = wait_memop[i].n - 1;
    }
}

WaitMemopLog *next_wait_memop(int objid, int version) {
    TLS_tid();
    int i;
    WaitMemopLog *log = wait_memop[objid].log;
    // Search if there's any read get the current version.
    DPRINTF("T%d W%d searching wait for obj %d @%d wait_memop_idx[%d] %d\n",
            tid, TLS(memop), objid, version, objid, TLS(wait_memop_idx)[objid]);
    for (i = TLS(wait_memop_idx)[objid]; i <= wait_memop[objid].size &&
            (version > log[i].version || log[i].tid == tid); ++i);

    if (i <= wait_memop[objid].size && version == log[i].version) {
        TLS(wait_memop_idx)[objid] = i + 1;
        return &log[i];
    }
    TLS(wait_memop_idx)[objid] = i;
    DPRINTF("T%d W%d No RD @%d for obj %d found wait_memop_idx[%d] = %d\n",
        tid, TLS(memop), version, objid, objid, i);
    return NULL;
}

void mem_init(int nthr) {
    load_wait_memop_log();
    ALLOC_TLS_GLOBAL(nthr, wait_memop_idx);
    ALLOC_TLS_GLOBAL(nthr, wait_version_log);
    ALLOC_TLS_GLOBAL(nthr, wait_version);
    ALLOC_TLS_GLOBAL(nthr, no_more_wait_version);
    ALLOC_TLS_GLOBAL(nthr, memop);

    obj_version = calloc_check(NOBJS, sizeof(*obj_version), "Can't allocate obj_version");
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

    TLS(wait_memop_idx) = calloc_check(NOBJS, sizeof(*TLS(wait_memop_idx)), "wait_memop_idx[tid]");

    TLS(wait_version_log) = open_log("log/version", tid);

    next_wait_version_log();
}

static void wait_version(int objid) {
    TLS_tid();

    if (!TLS(no_more_wait_version) && TLS(memop) == TLS(wait_version).memop) {
        // Wait version reaches the recorded value
        DPRINTF("T%d op%d wait obj %d @%d->%d\n", tid, TLS(memop), 
            objid, obj_version[objid], TLS(wait_version).version);
        while (obj_version[objid] < TLS(wait_version).version) {
            cpu_relax();
        }
        DPRINTF("T%d op%d wait done\n", tid, TLS(memop));

        if (obj_version[objid] != TLS(wait_version).version) {
            fprintf(stderr, "T%d obj_version[%d] = %d, wait_version = %d\n",
                tid, objid, obj_version[objid], TLS(wait_version).version);
        }
        assert(obj_version[objid] == TLS(wait_version).version);
        next_wait_version_log();
    }
}

int32_t mem_read(int tid, int32_t *addr) {
    int val;
    int objid = obj_id(addr);

    wait_version(objid);

    val = *addr;
    TLS(memop)++;

    return val;
}

void mem_write(int tid, int32_t *addr, int32_t val) {
    int objid = obj_id(addr);

    wait_version(objid);

    // Wait memory accesses that happen at this version.
    WaitMemopLog *log;
    while ((log = next_wait_memop(objid, obj_version[objid])) != NULL) {
        DPRINTF("T%d W%d wait T%d R%d on obj %d\n", tid, TLS(memop),
            log->tid, log->memop, objid);
        while (memop_tls[log->tid] <= log->memop) {
            cpu_relax();
        }
        DPRINTF("T%d W%d wait done\n", tid, TLS(memop));
    }

    *addr = val;
    obj_version[objid] += 1;
    TLS(memop)++;
}

void mem_finish_thr() {}
