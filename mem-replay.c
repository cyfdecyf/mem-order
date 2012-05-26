#include "mem.h"
#include "log.h"
#include "spinlock.h" // Just for barrier and cpu_relax
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define DEBUG

int *obj_version;

typedef struct WarLog {
    int read_memop;
    int version;
    int tid;
} WarLog;

static inline int read_war_log(FILE *log, WarLog *ent, int *objid) {
    if (fscanf(log, "%d %d %d %d", objid, &ent->version, &ent->read_memop,
            &ent->tid) != 4) {
        return 0;
    }
    return 1;
}

typedef struct {
    int read_memop;
    int objid;
    int version;
} RawLog;
DEFINE_TLS_GLOBAL(RawLog, read_aw);
DEFINE_TLS_GLOBAL(FILE *, read_aw_log);
DEFINE_TLS_GLOBAL(char, no_more_read_aw);

static inline void next_read_aw_log() {
    TLS_tid();
    RawLog *ent = &TLS(read_aw);

    int last_read; // Not used in replay
    if (fscanf(TLS(read_aw_log), "%d %d %d %d", &ent->objid, &ent->version,
            &ent->read_memop, &last_read) != 4) {
        fprintf(stderr, "No more RAW log for thread %d\n", tid);
        TLS(no_more_read_aw) = 1;
    }
}

typedef struct {
    int write_memop;
    int objid;
    int version;
} WawLog;
DEFINE_TLS_GLOBAL(WawLog, write_aw);
DEFINE_TLS_GLOBAL(FILE *, write_aw_log);
DEFINE_TLS_GLOBAL(char, no_more_write_aw);

static inline void next_write_aw_log() {
    TLS_tid();
    WawLog *ent = &TLS(write_aw);
    if (fscanf(TLS(write_aw_log), "%d %d %d", &ent->objid, &ent->version,
            &ent->write_memop) != 3) {
        fprintf(stderr, "No more WAW log for thread %d\n", tid);
        TLS(no_more_write_aw) = 1;
    }
}

typedef struct {
    WarLog *log;
    int n;
    int size;
} War;

War *war;
DEFINE_TLS_GLOBAL(int *, war_idx);

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
    }
#ifdef DEBUG
    fprintf(stderr, "war[0].size = %d\n", war[0].size);
#endif
}

WarLog *wait_read(int objid, int version) {
    TLS_tid();
    int i;
    WarLog *log = war[objid].log;
    // Search if there's any read get the current version.
#ifdef DEBUG
    fprintf(stderr, "T%d searching wait for obj %d @%d war_idx %d\n",
            tid, objid, version, TLS(war_idx)[objid]);
#endif
    for (i = TLS(war_idx)[objid]; i <= war[objid].size &&
            (version > log[i].version || log[i].tid == tid); ++i);
    // Ignore log that waiting self.
    if (i <= war[objid].size && version == log[i].version) {
        TLS(war_idx)[objid] = i + 1;
        return &log[i];
    }
    TLS(war_idx)[objid] = i;
#ifdef DEBUG
    fprintf(stderr, "No RD @%d for obj %d found\n", version, objid);
#endif
    return NULL;
}

DEFINE_TLS_GLOBAL(int, read_memop);
DEFINE_TLS_GLOBAL(int, write_memop);

void mem_init(int nthr) {
    load_war_log();
    ALLOC_TLS_GLOBAL(nthr, war_idx);
    ALLOC_TLS_GLOBAL(nthr, write_aw_log);
    ALLOC_TLS_GLOBAL(nthr, write_aw);
    ALLOC_TLS_GLOBAL(nthr, no_more_write_aw);
    ALLOC_TLS_GLOBAL(nthr, read_aw_log);
    ALLOC_TLS_GLOBAL(nthr, read_aw);
    ALLOC_TLS_GLOBAL(nthr, no_more_read_aw);
    ALLOC_TLS_GLOBAL(nthr, read_memop);
    ALLOC_TLS_GLOBAL(nthr, write_memop);

    obj_version = calloc_check(NOBJS, sizeof(*obj_version), "Can't allocate obj_version");
}

void mem_init_thr(int tid) {
    // Must set tid before using the tid_key
    pthread_setspecific(tid_key, (void *)(long)tid);

    TLS(war_idx) = calloc_check(NOBJS, sizeof(*TLS(war_idx)), "war_idx[tid]");

    TLS(write_aw_log) = open_log("log/rec-wr", tid);
    TLS(read_aw_log) = open_log("log/rec-rd", tid);

    next_read_aw_log();
    next_write_aw_log();
}

int32_t mem_read(int32_t *addr) {
    TLS_tid();
    int val;
    // First wait write
    int objid = obj_id(addr);

    if (!TLS(no_more_read_aw) && TLS(read_memop) == TLS(read_aw).read_memop) {
        // objid in log should have no use
        assert(objid == TLS(read_aw).objid);

        fprintf(stderr, "T%d R%d wait obj %d @%d->%d\n", tid, TLS(read_memop), 
            objid, obj_version[objid], TLS(read_aw).version);
        while (obj_version[objid] < TLS(read_aw).version) {
            cpu_relax();
        }
        fprintf(stderr, "wait done\n");

        if (obj_version[objid] != TLS(read_aw).version) {
            fprintf(stderr, "obj_version[%d] = %d, read_aw.version = %d\n", objid, obj_version[objid], TLS(read_aw).version);
        }
        assert(obj_version[objid] == TLS(read_aw).version);
        next_read_aw_log();
    }

    val = *addr;
    TLS(read_memop)++;

    return val;
}

void mem_write(int32_t *addr, int32_t val) {
    TLS_tid();
    int objid = obj_id(addr);

    // First wait write
    if (!TLS(no_more_write_aw) && TLS(write_memop) == TLS(write_aw).write_memop) {
        // objid in log should have no use
        assert(objid == TLS(write_aw).objid);

        fprintf(stderr, "T%d W%d wait obj %d @%d->@%d\n", tid, TLS(write_memop),
            objid, obj_version[objid], TLS(write_aw).version);
        while (obj_version[objid] < TLS(write_aw).version) {
            cpu_relax();
        }
        fprintf(stderr, "wait done\n");

        assert(obj_version[objid] == TLS(write_aw).version);
        next_write_aw_log();
    }

    // Next wait read that get this version.
    WarLog *log;
    while ((log = wait_read(objid, obj_version[objid])) != NULL) {
        fprintf(stderr, "T%d W%d wait T%d R%d on obj %d\n", tid, TLS(write_memop),
            log->tid, log->read_memop, objid);
        while (read_memop_tls[log->tid] <= log->read_memop) {
            cpu_relax();
        }
        fprintf(stderr, "wait done\n");
    }

    *addr = val;
    obj_version[objid] += 1;
    TLS(write_memop)++;
}

void mem_finish_thr() {}
