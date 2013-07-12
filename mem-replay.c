#include "mem.h"
#include "log.h"
#include "spinlock.h" // Just for barrier and cpu_relax
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

// #define DEBUG
#include "debug.h"

version_t *obj_version;

__thread memop_t memop;
memop_t **memop_cnt;

__thread WaitVersion wait_version;

#ifdef BINARY_LOG
__thread MappedLog wait_version_log;

static inline void next_wait_version_log() {
    MappedLog *log = &wait_version_log;
    WaitVersion *wv = (WaitVersion *)(log->buf);

    if (wv->memop == -1) {
        wait_version.memop = -1;
        return;
    }

    wait_version = *wv;
    log->buf = (char *)(wv + 1);
}

#else // BINARY_LOG
__thread FILE *wait_version_log;

static inline void next_wait_version_log() {
    WaitVersion *ent = &wait_version;

    if (fscanf(wait_version_log, "%d %d", &ent->version,
            &ent->memop) != 2) {
        // fprintf(stderr, "No more wait version log for thread %d\n", tid);
    }
}
#endif // BINARY_LOG

typedef struct {
    ReplayWaitMemop *log;
    int n;
    int size;

    // For debugging, check whether there's concurrent access. More details in
    // next_wait_memop().
    pthread_mutex_t mutex;
} ReplayWaitMemopLog;

ReplayWaitMemopLog *wait_memop_log;

#ifdef BINARY_LOG

static void load_wait_memop_log() {
    MappedLog memop_log, index_log;
    wait_memop_log = calloc_check(NOBJS, sizeof(*wait_memop_log), "Can't allocate wait_memop");

    if (open_mapped_log_path(LOGDIR"memop", &memop_log) != 0) {
        DPRINTF("Can't open memop log\n");
        for (int i = 0; i < NOBJS; i++) {
            wait_memop_log[i].log = NULL;
            wait_memop_log[i].n = 0;
            wait_memop_log[i].size = -1;
        }
        return;
    }

    if (open_mapped_log_path(LOGDIR"memop-index", &index_log) != 0) {
        printf("Can't open memop-index log\n");
        exit(1);
    }
    int *index = (int *)index_log.buf;

    ReplayWaitMemop *log_start = (ReplayWaitMemop *)memop_log.buf;
    for (int i = 0; i < NOBJS; i++) {
        wait_memop_log[i].n = 0;
        pthread_mutex_init(&wait_memop_log[i].mutex, NULL);
        if (*index == -1) {
            wait_memop_log[i].log = NULL;
            wait_memop_log[i].size = -1;
            index += 2;
            continue;
        }
        DPRINTF("wait_memop_log[%d] index = %d size = %d\n", i,
            *index, *(index + 1));
        wait_memop_log[i].log = &log_start[*index++];
        wait_memop_log[i].size = *index++;
    }
    unmap_log(&index_log);
}

#else // BINARY_LOG

enum { INIT_LOG_CNT = 1000 };

static inline int read_wait_memop_log(FILE *log, ReplayWaitMemop *ent, objid_t *objid) {
    if (fscanf(log, "%d %d %d %hhd", objid, &ent->version, &ent->memop,
            &ent->tid) != 4) {
        return 0;
    }
    return 1;
}

static void load_wait_memop_log() {
    FILE *logfile = fopen("memop", "r");
    if (! logfile) {
        printf("Can't open memop\n");
        exit(1);
    }

    wait_memop_log = calloc_check(NOBJS, sizeof(*wait_memop_log), "Can't allocate wait_memop");

    int wait_memoplogsize = INIT_LOG_CNT * sizeof(wait_memop_log[0].log[0]);
    for (int i = 0; i < NOBJS; i++) {
        wait_memop_log[i].log = calloc_check(1, wait_memoplogsize, "Can't allocate wait_memop[i].log");
        wait_memop_log[i].size = INIT_LOG_CNT;
        wait_memop_log[i].n = 0;
    }

    objid_t objid;
    ReplayWaitMemop ent;
    while (fscanf(logfile, "%d %d %d %hhd", &objid, &ent.version, &ent.memop, &ent.tid) == 4) {
        assert(objid < NOBJS);

        int n = wait_memop_log[objid].n;
        // Need to enlarge log array
        if (n >= wait_memop_log[objid].size) {
            unsigned int mem_size = wait_memop_log[objid].size * sizeof(wait_memop_log[0].log[0]) * 2;
            ReplayWaitMemop *new_log = realloc(wait_memop_log[objid].log, mem_size);
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

ReplayWaitMemop *next_wait_memop(objid_t objid) {
    // There should be no concurrent access to an object's memop log.
    // The mutex is a assertion on this.
    if (pthread_mutex_trylock(&wait_memop_log[objid].mutex) != 0) {
        printf("concurrent access to same object's memop log\n");
        abort();
    }
    if (wait_memop_log[objid].n >= wait_memop_log[objid].size) {
        /*DPRINTF("T%d W%d B%d no more wait memop log\n", tid, memop, objid);*/
        pthread_mutex_unlock(&wait_memop_log[objid].mutex);
        return NULL;
    }

    int i;
    ReplayWaitMemop *log = wait_memop_log[objid].log;
    version_t version = obj_version[objid];
    // Search if there's any read get the current version.
    DPRINTF("T%hhd W%d B%d wait memop search for X @%d idx[%d] = %d\n",
            tid, memop, objid, version, objid, wait_memop_log[objid][n]);
    // XXX As memop log index is shared among cores, can't skip log with the
    // same tid to the checking core itself because other cores need this log
    for (i = wait_memop_log[objid].n;
        i < wait_memop_log[objid].size && version > log[i].version;
        i++);

    if (i < wait_memop_log[objid].size && version == log[i].version) {
        // This log is used, so start from next one on next scan.
        wait_memop_log[objid].n = i + 1;
        pthread_mutex_unlock(&wait_memop_log[objid].mutex);
        return &log[i];
    }
    wait_memop_log[objid].n = i;
    DPRINTF("T%d W%d B%d wait memop No X @%d found idx[%d] = %d\n",
        tid, memop, objid, version, objid, i);
    pthread_mutex_unlock(&wait_memop_log[objid].mutex);
    return NULL;
}

void mem_init(tid_t nthr) {
    load_wait_memop_log();

    obj_version = calloc_check(NOBJS, sizeof(*obj_version), "Can't allocate obj_version");
    memop_cnt = calloc_check(nthr, sizeof(*memop_cnt), "Can't allocate memop_cnt");
}

void mem_init_thr(tid_t tid) {
    memop_cnt[tid] = &memop;

#ifdef BINARY_LOG
    if (open_mapped_log("version", tid, &wait_version_log) != 0) {
        printf("T%d Error opening version log\n", (int)tid);
        exit(1);
    }
#else
    wait_version_log = open_log("version", tid);
#endif

    next_wait_version_log();
}

static void wait_object_version(int objid, const char op) {
    if (memop == wait_version.memop) {
        // Wait version reaches the recorded value
        DPRINTF("T%hhd %c%d B%d wait version @%d->%d\n", tid, op, memop,
            objid, obj_version[objid], wait_version.version);
        while (obj_version[objid] < wait_version.version) {
            cpu_relax();
        }
        DPRINTF("T%d %c%d B%d wait version done\n", tid, op, memop, objid);

        if (obj_version[objid] != wait_version.version) {
            fprintf(stderr, "T%d obj_version[%d] = %d, wait_version = %d\n",
                (int)tid, (int)objid, (int)obj_version[objid], (int)wait_version.version);
        }
        assert(obj_version[objid] == wait_version.version);
        next_wait_version_log();
    }
}

int32_t mem_read(tid_t tid, int32_t *addr) {
    int val;
    objid_t objid = obj_id(addr);

    wait_object_version(objid, 'R');

    val = *addr;
    memop++;

    return val;
}

void mem_write(tid_t tid, int32_t *addr, int32_t val) {
    int objid = obj_id(addr);

    wait_object_version(objid, 'W');

    // Wait memory accesses that happen at this version.
    ReplayWaitMemop *log;
    while ((log = next_wait_memop(objid)) != NULL) {
        DPRINTF("T%d W%d B%d wait memop T%d X%d\n", tid, memop, objid,
            log->tid, log->memop);
        while (*memop_cnt[log->tid] <= log->memop) {
            cpu_relax();
        }
        DPRINTF("T%d W%d B%d wait memop done\n", tid, memop, objid);
    }

    *addr = val;
    obj_version[objid] += 1;
    memop++;
}

void mem_finish_thr() {}
