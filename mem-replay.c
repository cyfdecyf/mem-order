#include "mem.h"
#include "log.h"
#include "spinlock.h" // Just for barrier and cpu_relax
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

/*#define DEBUG*/
#include "debug.h"

version_t *obj_version;

__thread memop_t memop;
memop_t **memop_cnt;

__thread struct wait_version wait_version;

__thread struct mapped_log wait_version_log;

static inline void next_wait_version_log() {
    struct mapped_log *log = &wait_version_log;
    struct wait_version *wv = (struct wait_version *)(log->buf);

    if (wv->memop == -1) {
        wait_version.memop = -1;
        return;
    }

    wait_version = *wv;
    log->buf = (char *)(wv + 1);
}

struct replay_wait_memop_log {
    struct replay_wait_memop *log;
    int n;
    int size;

    // For debugging, check whether there's concurrent access. More details in
    // next_reader_memop().
    pthread_mutex_t mutex;
};

struct replay_wait_memop_log *wait_reader_log;

static void load_wait_reader_log() {
    struct mapped_log memop_log, index_log;
    wait_reader_log = calloc_check(g_nobj, sizeof(*wait_reader_log), "Can't allocate wait_memop");

    if (open_mapped_log_path(LOGDIR"memop", &memop_log) != 0) {
        DPRINTF("Can't open memop log\n");
        for (int i = 0; i < g_nobj; i++) {
            wait_reader_log[i].log = NULL;
            wait_reader_log[i].n = 0;
            wait_reader_log[i].size = -1;
        }
        return;
    }

    if (open_mapped_log_path(LOGDIR"memop-index", &index_log) != 0) {
        printf("Can't open memop-index log\n");
        exit(1);
    }
    int *index = (int *)index_log.buf;

    struct replay_wait_memop *log_start = (struct replay_wait_memop *)memop_log.buf;
    for (int i = 0; i < g_nobj; i++) {
        wait_reader_log[i].n = 0;
        pthread_mutex_init(&wait_reader_log[i].mutex, NULL);
        if (*index == -1) {
            wait_reader_log[i].log = NULL;
            wait_reader_log[i].size = -1;
            index += 2;
            continue;
        }
        DPRINTF("wait_reader_log[%d] index = %d size = %d\n", i,
            *index, *(index + 1));
        wait_reader_log[i].log = &log_start[*index++];
        wait_reader_log[i].size = *index++;
    }
    unmap_log(&index_log);
}

struct replay_wait_memop *next_reader_memop(objid_t objid) {
    // There should be no concurrent access to an object's memop log.
    // The mutex is a assertion on this.
    if (pthread_mutex_trylock(&wait_reader_log[objid].mutex) != 0) {
        printf("concurrent access to same object's memop log\n");
        abort();
    }
    if (wait_reader_log[objid].n >= wait_reader_log[objid].size) {
        /*DPRINTF("T%d W%d B%d no more wait reader log\n", tid, memop, objid);*/
        pthread_mutex_unlock(&wait_reader_log[objid].mutex);
        return NULL;
    }

    int i;
    struct replay_wait_memop *log = wait_reader_log[objid].log;
    version_t version = obj_version[objid];
    // Search if there's any read get the current version.
    DPRINTF("T%hhd W%ld B%d wait reader search for X @%ld idx[%d] = %d\n",
            g_tid, memop, objid, version, objid, wait_reader_log[objid].n);
    // XXX As memop log index is shared among cores, can't skip log with the
    // same tid to the checking core itself because other cores need this log
    for (i = wait_reader_log[objid].n;
        i < wait_reader_log[objid].size && version > log[i].version;
        i++);

    if (i < wait_reader_log[objid].size && version == log[i].version) {
        // This log is used, so start from next one on next scan.
        wait_reader_log[objid].n = i + 1;
        pthread_mutex_unlock(&wait_reader_log[objid].mutex);
        return &log[i];
    }
    wait_reader_log[objid].n = i;
    DPRINTF("T%d W%ld B%d wait reader No X @%ld found idx[%d] = %d\n",
        g_tid, memop, objid, version, objid, i);
    pthread_mutex_unlock(&wait_reader_log[objid].mutex);
    return NULL;
}

#ifdef DEBUG_ACCESS
__thread struct mapped_log debug_access_log;

#define MEMACC_ERR(fmt, ...) \
    do { \
        printf("T%d: %c%ld " fmt "\n", g_tid, acc, memop, ##__VA_ARGS__); \
        err = true; \
    } while(0)

static inline void check_access(char acc, objid_t objid, version_t ver, uint32_t val) {
    bool err = false;
    struct mem_acc *ent = (struct mem_acc *)read_log_entry(&debug_access_log, sizeof(*ent));
    if (ent->acc != acc) {
        MEMACC_ERR("memacc type error");
    }
    if (ent->objid != objid) {
        MEMACC_ERR("memacc objid error rec: %d replay: %d", ent->objid, objid);
    }
    if (ent->val != val) {
        MEMACC_ERR("memacc val error rec: %d replay: %d", ent->val, val);
    }
    if (ent->version != ver) {
        MEMACC_ERR("memacc ver error rec: %ld replay: %ld", ent->version, ver);
    }
    if (ent->memop != memop) {
        MEMACC_ERR("memacc memop error rec: %ld replay: %ld", ent->memop, memop);
    }
    if (err) {
        exit(1);
    }
}
#endif

void mem_init(tid_t nthr, int nobj) {
    g_nobj = nobj;
    load_wait_reader_log();

    obj_version = calloc_check(g_nobj, sizeof(*obj_version), "Can't allocate obj_version");
    memop_cnt = calloc_check(nthr, sizeof(*memop_cnt), "Can't allocate memop_cnt");
}

void mem_init_thr(tid_t tid) {
    memop_cnt[tid] = &memop;

    if (open_mapped_log("version", tid, &wait_version_log) != 0) {
        printf("T%d Error opening version log\n", (int)tid);
        exit(1);
    }

#ifdef DEBUG_ACCESS
    if (open_mapped_log("debug-access", tid, &debug_access_log) != 0) {
        printf("T%d Error opening debug-access log\n", (int)tid);
        exit(1);
    }
#endif

    next_wait_version_log();
}

static void wait_object_version(int objid, const char op) {
    if (memop == wait_version.memop) {
        // Wait version reaches the recorded value
        DPRINTF("T%hhd %c%ld B%d wait version @%ld->%ld\n", g_tid, op, memop,
            objid, obj_version[objid], wait_version.version);
        while (obj_version[objid] < wait_version.version) {
            cpu_relax();
        }
        DPRINTF("T%d %c%ld B%d wait version done\n", g_tid, op, memop, objid);

        if (obj_version[objid] != wait_version.version) {
            fprintf(stderr, "T%d obj_version[%d] = %d, wait_version = %d\n",
                (int)g_tid, (int)objid, (int)obj_version[objid], (int)wait_version.version);
        }
        assert(obj_version[objid] == wait_version.version);
        next_wait_version_log();
    }
}

static void wait_reader(int objid) {
    // Wait memory accesses that happen at this version.
    struct replay_wait_memop *log;
    while ((log = next_reader_memop(objid)) != NULL) {
        DPRINTF("T%d W%ld B%d wait reader T%d X%ld\n", g_tid, memop, objid,
            log->tid, log->memop);
        while (*memop_cnt[log->tid] <= log->memop) {
            cpu_relax();
        }
        DPRINTF("T%d W%ld B%d wait reader done\n", g_tid, memop, objid);
    }
}

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    int val;
    objid_t objid = calc_objid(addr);

    wait_object_version(objid, 'R');

    DPRINTF("T%d R%ld B%d @%ld\n", g_tid, memop, objid, obj_version[objid]);
    val = *addr;

#ifdef DEBUG_ACCESS
    check_access('R', objid, obj_version[objid], val);
#endif

    memop++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    int objid = calc_objid(addr);

    wait_object_version(objid, 'W');
    wait_reader(objid);

    DPRINTF("T%d W%ld B%d @%ld\n", g_tid, memop, objid, obj_version[objid]);
#ifdef DEBUG_ACCESS
    check_access('W', objid, obj_version[objid], val);
#endif
    *addr = val;
    obj_version[objid] += 2;
    memop++;
}

void mem_finish_thr() {}
