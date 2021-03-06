#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <execinfo.h>
#include <signal.h>

// g_obj are aligned to OBJ_SIZE
int64_t *g_obj;

#define NOBJS 10

objid_t calc_objid_addcnt(void *addr) {
    return ((long)addr - (long)g_obj) >> 3;
}

void print_g_obj(void) {
    for (int i = 0; i < NOBJS; i++) {
        printf("%lx\n", (long)g_obj[i]);
    }
}

static void handler(int sig)
{
    void *array[10];
    size_t size;

    /* get void*'s for all entries on the stack */
    size = backtrace(array, 10);

    /* print out all the frames to stderr */
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, 2);
    exit(1);
}

static int nthr;
static volatile int start_flag;

static void sync_thread(volatile int *flag) {
    __sync_fetch_and_add(flag, 1);
    while (*flag != nthr)
        asm volatile ("pause");
}

static inline void thr_start(void *_tid) {
    // Must set tid before using
    mem_init_thr((tid_t)(long)_tid);
    sync_thread(&start_flag);
}

static inline void thr_end() {
    mem_finish_thr();
}

#define NITER 20000

static void *access_thr_fn1(void *dummyid) {
    thr_start(dummyid);

    for (int i = 0; i < NITER; i++) {
        for (int j = 0; j < NOBJS; j++) {
            // First read, then write. From object 0 to NOBJS-1
            uint32_t *addr = (uint32_t *)&g_obj[j];
            uint32_t val = mem_read(g_tid, addr);
            mem_write(g_tid, addr, val + 1);
        }
    }

    thr_end();
    return NULL;
}

static void *access_thr_fn2(void *dummyid) {
    thr_start(dummyid);

    for (int i = 0; i < NITER; i++) {
        for (int j = NOBJS - 1; j > -1; j--) {
            // First read, then write. From object NOBJS-1 to 0
            uint32_t *addr = (uint32_t *)&g_obj[j];
            uint32_t val = mem_read(g_tid, addr);
            mem_write(g_tid, addr, val + 1);
        }
    }

    thr_end();
    return NULL;
}

static void *access_thr_fn3(void *dummyid) {
    thr_start(dummyid);

    for (int i = 0; i < NITER; i++) {
        for (int j = 0; j < NOBJS - 1; j += 2) {
            // First read memobj j, the write to memobj j+1, then write to memobj j
            uint32_t *addrj = (uint32_t *)&g_obj[j];
            uint32_t *addrj1 = (uint32_t *)&g_obj[j+1];

            uint32_t val = mem_read(g_tid, addrj);
            mem_write(g_tid, addrj1, val + 1);

            val = mem_read(g_tid, addrj1);
            mem_write(g_tid, addrj, val + 2);
        }
    }

    thr_end();
    return NULL;
}

static void *access_thr_fn4(void *dummyid) {
    thr_start(dummyid);

    for (int i = 0; i < NITER; i++) {
        for (int j = NOBJS - 1; j > 0; j -= 2) {
            // First read memobj j, the write to memobj j-1, then write to memobj j
            uint32_t *addrj = (uint32_t *)&g_obj[j];
            uint32_t *addrj1 = (uint32_t *)&g_obj[j-1];

            uint32_t val = mem_read(g_tid, addrj);
            mem_write(g_tid, addrj1, val + 1);

            val = mem_read(g_tid, addrj1);
            mem_write(g_tid, addrj, val + 2);
        }
    }

    thr_end();
    return NULL;
}

typedef void *(*thr_fn_t)(void *dummyid);
static thr_fn_t thr_fn[] = {
    access_thr_fn1,
    access_thr_fn2,
    access_thr_fn3,
    access_thr_fn4,
};

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <no of threads>\n", argv[0]);
        exit(1);
    }

    if (posix_memalign((void **)&g_obj, sizeof(*g_obj), NOBJS * sizeof(*g_obj)) != 0) {
        printf("memory allocation for objs failed\n");
        exit(1);
    }
    calc_objid = calc_objid_addcnt;

    signal(SIGSEGV, handler);

    nthr = atoi(argv[1]);
    pthread_t *thr;
    thr = calloc_check(nthr, sizeof(pthread_t), "pthread_t array thr");

    // Initialize memory order recorder/replayer
    mem_init((tid_t)nthr, NOBJS);

    for (long i = 0; i < nthr; i++) {
        if (pthread_create(&thr[i], NULL, thr_fn[i % sizeof(thr_fn)/sizeof(thr_fn[0])], (void *)i) != 0) {
            printf("thread creation failed\n");
            exit(1);
        }
    }

    for (long i = 0; i < nthr; i++) {
        pthread_join(thr[i], NULL);
    }

    print_g_obj();
    return 0;
}
