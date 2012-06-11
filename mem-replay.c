#include "mem.h"
#include "log.h"
#include "spinlock.h" // Just for barrier and cpu_relax
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// #define DEBUG
#include "debug.h"

int *obj_version;

DEFINE_TLS_GLOBAL(int, memop);

typedef struct {
    int memop;
    int version;
} WaitVersion;
DEFINE_TLS_GLOBAL(WaitVersion, wait_version);

#ifdef BINARY_LOG
DEFINE_TLS_GLOBAL(MappedLog, wait_version_log);

static inline void next_wait_version_log() {
    TLS_tid();
    MappedLog *log = &TLS(wait_version_log);

    if (*(int *)log->buf == -1) {
        DPRINTF("T%d no more wait version, cnt = %d\n", tid, version_log_cnt);
        TLS(wait_version).memop = -1;
        return;
    }

    TLS(wait_version) = *(WaitVersion *)(log->buf);
    log->buf += sizeof(WaitVersion);

    if (log->buf + sizeof(WaitVersion) > log->end) {
        log->buf = log->end;
        log->end = log->buf + LOG_BUFFER_SIZE;
    }
}

#else // BINARY_LOG
DEFINE_TLS_GLOBAL(FILE *, wait_version_log);

static inline void next_wait_version_log() {
    TLS_tid();
    WaitVersion *ent = &TLS(wait_version);

    if (fscanf(TLS(wait_version_log), "%d %d", &ent->version,
            &ent->memop) != 2) {
        // fprintf(stderr, "No more wait version log for thread %d\n", tid);
    }
}
#endif // BINARY_LOG

typedef struct {
    // Order of field must match with binary log
    int version;
    int memop;
    int tid;
} WaitMemop;

typedef struct {
    WaitMemop *log;
    int n;
    int size;
} WaitMemopLog;

WaitMemopLog *wait_memop_log;
int *wait_memop_idx;

#ifdef BINARY_LOG

static void load_wait_memop_log() {
    MappedLog memop_log, index_log;
    if (open_mapped_log_path("log/memop", &memop_log) != 0) {
        printf("Error opening memop log\n");
        exit(1);
    }
    if (open_mapped_log_path("log/memop-index", &index_log) != 0) {
        printf("Error opening memop log\n");
        exit(1);
    }

    wait_memop_log = calloc_check(NOBJS, sizeof(*wait_memop_log), "Can't allocate wait_memop");

    int *index = (int *)index_log.buf;
    WaitMemop *log_start = (WaitMemop *)memop_log.buf;
    for (int i = 0; i < NOBJS; i++) {
        if (*index == -1) {
            wait_memop_log[i].log = NULL;
            wait_memop_log[i].n = 0;
            wait_memop_log[i].size = -1;
            index += 2;
            continue;
        }
        DPRINTF("wait_memop_log[%d] index = %d size = %d\n", i,
            *index, *(index + 1));
        wait_memop_log[i].log = &log_start[*index++];
        wait_memop_log[i].n = 0;
        wait_memop_log[i].size = *index++;
#ifdef DEBUG
        WaitMemop *log = wait_memop_log[i].log;
        for(int j = 0; j < wait_memop_log[i].size; ++j){
            DPRINTF("%d %d %d %d\n", log[j].objid, log[j].version,
                log[j].memop, log[j].tid);
        }
#endif
    }
    unmap_log(index_log.buf, index_log.end - index_log.buf);
}

#else // BINARY_LOG

enum { INIT_LOG_CNT = 1000 };

static inline int read_wait_memop_log(FILE *log, WaitMemop *ent, int *objid) {
    if (fscanf(log, "%d %d %d %d", objid, &ent->version, &ent->memop,
            &ent->tid) != 4) {
        return 0;
    }
    return 1;
}

static void load_wait_memop_log() {
    FILE *logfile = fopen("log/memop", "r");
    if (! logfile) {
        printf("Can't open log/memop\n");
        exit(1);
    }

    wait_memop_log = calloc_check(NOBJS, sizeof(*wait_memop_log), "Can't allocate wait_memop");

    int wait_memoplogsize = INIT_LOG_CNT * sizeof(wait_memop_log[0].log[0]);
    for (int i = 0; i < NOBJS; i++) {
        wait_memop_log[i].log = calloc_check(1, wait_memoplogsize, "Can't allocate wait_memop[i].log");
        wait_memop_log[i].size = INIT_LOG_CNT;
        wait_memop_log[i].n = 0;
    }

    int objid;
    WaitMemop ent;
    while (fscanf(logfile, "%d %d %d %d", &objid, &ent.version, &ent.memop, &ent.tid) == 4) {
        assert(objid < NOBJS);

        int n = wait_memop_log[objid].n;
        // Need to enlarge log array
        if (n >= wait_memop_log[objid].size) {
            unsigned int mem_size = wait_memop_log[objid].size * sizeof(wait_memop_log[0].log[0]) * 2;
            WaitMemop *new_log = realloc(wait_memop_log[objid].log, mem_size);
            if (! new_log) {
                printf("Can't reallocate for wait_memop[%d].log\n", objid);
                exit(1);
            }
            wait_memop_log[objid].log = new_log;
            wait_memop_log[objid].size *= 2;
        }

        wait_memop_log[objid].log[n] = ent;
        wait_memop_log[objid].n++;
    }

    for (int i = 0; i < NOBJS; ++i) {
        // size is then used as index to the last log.
        // n is then used as the index to the next unused log
        wait_memop_log[i].size = wait_memop_log[i].n - 1;
    }
}
#endif // BINARY_LOG

WaitMemop *next_wait_memop(int objid, int version) {
    TLS_tid();
    int i;
    WaitMemop *log = wait_memop_log[objid].log;
    // Search if there's any read get the current version.
    DPRINTF("T%d W%d B%d wait memop search for X @%d wait_memop_idx[%d] = %d\n",
            tid, TLS(memop), objid, version, objid, wait_memop_idx[objid]);
    for (i = wait_memop_idx[objid]; i <= wait_memop_log[objid].size &&
            (version > log[i].version || log[i].tid == tid); ++i);

    if (i <= wait_memop_log[objid].size && version == log[i].version) {
        wait_memop_idx[objid] = i + 1;
        return &log[i];
    }
    DPRINTF("T%d W%d B%d wait memop No X @%d found wait_memop_idx[%d] = %d\n",
        tid, TLS(memop), objid, version, objid, i);
    return NULL;
}

void mem_init(int nthr) {
    load_wait_memop_log();
    wait_memop_idx = calloc_check(NOBJS, sizeof(*wait_memop_idx), "wait_memop_idx[tid]");
    ALLOC_TLS_GLOBAL(nthr, wait_version_log);
    ALLOC_TLS_GLOBAL(nthr, wait_version);
    ALLOC_TLS_GLOBAL(nthr, memop);

    obj_version = calloc_check(NOBJS, sizeof(*obj_version), "Can't allocate obj_version");
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

#ifdef BINARY_LOG
    if (open_mapped_log("log/version", tid, &TLS(wait_version_log)) != 0) {
        printf("T%d Error opening version log\n", tid);
        exit(1);
    }
    // Use end as the log buffer end. This is somewhat hacky.
    TLS(wait_version_log).end = TLS(wait_version_log).buf + LOG_BUFFER_SIZE;
#else
    TLS(wait_version_log) = open_log("log/version", tid);
#endif

    next_wait_version_log();
}

static void wait_version(int objid, const char op) {
    TLS_tid();

    if (TLS(memop) == TLS(wait_version).memop) {
        // Wait version reaches the recorded value
        DPRINTF("T%d %c%d B%d wait version @%d->%d\n", tid, op, TLS(memop),
            objid, obj_version[objid], TLS(wait_version).version);
        while (obj_version[objid] < TLS(wait_version).version) {
            cpu_relax();
        }
        DPRINTF("T%d %c%d B%d wait version done\n", tid, op, TLS(memop), objid);

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

    wait_version(objid, 'R');

    val = *addr;
    TLS(memop)++;

    return val;
}

void mem_write(int tid, int32_t *addr, int32_t val) {
    int objid = obj_id(addr);

    wait_version(objid, 'W');

    // Wait memory accesses that happen at this version.
    WaitMemop *log;
    while ((log = next_wait_memop(objid, obj_version[objid])) != NULL) {
        DPRINTF("T%d W%d B%d wait memop T%d X%d\n", tid, TLS(memop), objid,
            log->tid, log->memop);
        while (memop_tls[log->tid] <= log->memop) {
            cpu_relax();
        }
        DPRINTF("T%d W%d B%d wait memop done\n", tid, TLS(memop), objid);
    }

    *addr = val;
    obj_version[objid] += 1;
    TLS(memop)++;
}

void mem_finish_thr() {}
