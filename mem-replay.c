#include "mem.h"
#include "log.h"
#include "spinlock.h" // Just for barrier and cpu_relax
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int *obj_version;

typedef struct WarLog {
    int read_memop;
    int version;
    int tid;
} WarLog;

static inline int read_war_log(FILE *log, WarLog *ent, int *objid) {
    if (fscanf(log, "%d %d %d %d", objid,
        &ent->version, &ent->read_memop, &ent->tid) != 4) {
        return 0;
    }
    return 1;
}

typedef struct {
    int read_memop;
    int objid;
    int version;
} RawLog;
DEFINE_TLS_GLOBAL(RawLog, raw);
DEFINE_TLS_GLOBAL(FILE *, raw_log);

static inline int read_raw_log(FILE *log, RawLog *ent) {
    int last_read; // Not used in replay
    (void)last_read;
    if (fscanf(log, "%d %d %d %d", &ent->read_memop,
                &ent->objid, &ent->version, &last_read) != 4) {
        TLS_tid();
        fprintf(stderr, "No more RAW log for thread %d\n", tid);
        return 0;
    }
    return 1;
}

typedef struct {
    int write_memop;
    int objid;
    int version;
} WawLog;
DEFINE_TLS_GLOBAL(WawLog, waw);
DEFINE_TLS_GLOBAL(FILE *, waw_log);

static inline int read_waw_log(FILE *log, WawLog *ent) {
    if (fscanf(log, "%d %d %d", &ent->write_memop, &ent->objid, &ent->version) != 2) {
        TLS_tid();
        fprintf(stderr, "No more WAW log for thread %d\n", tid);
        return 0;
    }
    return 1;
}

typedef struct {
    WarLog *log;
    int n;
    int size;
} War;

War *war;

const int INIT_LOG_CNT = 1000;

static void load_war_log() {
    FILE *logfile = fopen("log/war", "r");
    if (! logfile) {
        printf("Can't open log/war\n");
        exit(1);
    }

    war = calloc_check(NOBJS, sizeof(*war), "Can't allocate war");
    for (int i = 0; i < NOBJS; i++) {
        war[i].log = calloc_check(INIT_LOG_CNT, sizeof(war[0].log[0]),
            "Can't allocate war[i].log");
        war[i].size = INIT_LOG_CNT;
    }

    WarLog ent;
    int objid;
    while (read_war_log(logfile, &ent, &objid)) {
        // Need to enlarge log array
        if (war[objid].n > war[objid].size) {
            printf("%d log resizing\n", objid);
            unsigned int new_size = sizeof(war[0].log[0]) * war[objid].size * 2;
            WarLog *new_log = realloc(war[objid].log, new_size);
            if (! new_log) {
                printf("Can't reallocate for war[%d].log\n", objid);
                exit(1);
            }
            war[objid].log = new_log;
            war[objid].size = new_size;
        }

        war[objid].log[war->n] = ent;
        war->n++;
    }

    for (int i = 0; i < NOBJS; ++i) {
        // cap is then used as index to the last log.
        // n is then used as the index to the next unused log
        war[i].size = war[i].n - 1;
        war[i].n = 0;
    }
}

WarLog *wait_read(int objid, int version) {
    int i;
    WarLog *log = war[objid].log;
    for (i = war[objid].n; version < log[i].version && i <= war[objid].size; ++i) {
    }
    if (i <= war[objid].size && version == log[i].version) {
        // Update next used log.
        // Only one write thread will execute this. So no lock is needed.
        war[objid].n = i + 1;
        return &log[i];
    }
    return NULL;
}

DEFINE_TLS_GLOBAL(int, read_memop);
DEFINE_TLS_GLOBAL(int, write_memop);

void mem_init(int nthr) {
    load_war_log();
    ALLOC_TLS_GLOBAL(nthr, waw_log);
    ALLOC_TLS_GLOBAL(nthr, waw);
    ALLOC_TLS_GLOBAL(nthr, raw_log);
    ALLOC_TLS_GLOBAL(nthr, raw);
    ALLOC_TLS_GLOBAL(nthr, read_memop);
    ALLOC_TLS_GLOBAL(nthr, write_memop);

    obj_version = calloc_check(NOBJS, sizeof(*obj_version), "Can't allocate obj_version");
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

    TLS(waw_log) = open_log("log/rec-wr", tid);
    TLS(raw_log) = open_log("log/rec-rd", tid);

    read_raw_log(TLS(raw_log), &TLS(raw));
    read_waw_log(TLS(waw_log), &TLS(waw));
}

int32_t mem_read(int32_t *addr) {
    int val;
    TLS_tid();
    // First wait write
    int objid = obj_id(addr);

    if (TLS(read_memop) == TLS(raw).read_memop) {
        // objid in log should have no use
        assert(objid == TLS(raw).objid);

        while (obj_version[objid] < TLS(raw).version) {
            cpu_relax();
        }

        assert(obj_version[objid] == TLS(raw).version);
        read_raw_log(TLS(raw_log), &TLS(raw));
    }

    val = *addr;
    TLS(read_memop)++;

    return val;
}

void mem_write(int32_t *addr, int32_t val) {
    TLS_tid();
    int objid = obj_id(addr);

    // First wait write
    if (TLS(write_memop) == TLS(waw).write_memop) {
        // objid in log should have no use
        assert(objid == TLS(waw).objid);

        while (obj_version[objid] < TLS(waw).version) {
            cpu_relax();
        }

        assert(obj_version[objid] == TLS(waw).version);
        read_waw_log(TLS(waw_log), &TLS(waw));
    }

    // Next wait read that get this version.
    WarLog *log;
    while ((log = wait_read(objid, obj_version[objid])) != NULL) {
        while (read_memop_tls[log->tid] <= log->read_memop) {
            cpu_relax();
        }
    }

    *addr = val;
    obj_version[objid] += 2;
    TLS(write_memop)++;
}
