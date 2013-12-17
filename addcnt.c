#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <execinfo.h>
#include <signal.h>

// objs are aligned to OBJ_SIZE
int64_t *objs;

objid_t obj_id_addcnt(void *addr) {
    return ((long)addr - (long)objs) >> 3;
}

void print_objs(void) {
    for (int i = 0; i < NOBJS; i++) {
        printf("%lx\n", (long)objs[i]);
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
static volatile int finish_flag;

static void sync_thread(volatile int *flag) {
    __sync_fetch_and_add(flag, 1);
    while (*flag != nthr)
        asm volatile ("pause");
}

static inline void thr_start(void *dummyid) {
    // Must set tid before using
    tid = (tid_t)(long)dummyid;
    mem_init_thr(tid);
    sync_thread(&start_flag);
}

static inline void thr_end() {
    sync_thread(&finish_flag);
    mem_finish_thr();
}

static void *access_thr_fn1(void *dummyid) {
    thr_start(dummyid);

    for (int i = 0; i < NITER; i++) {
        for (int j = 0; j < NOBJS; j++) {
            // First read, then write. From object 0 to NOBJS-1
            uint32_t *addr = (uint32_t *)&objs[j];
            uint32_t val = mem_read(tid, addr);
            mem_write(tid, addr, val + 1);
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
            uint32_t *addr = (uint32_t *)&objs[j];
            uint32_t val = mem_read(tid, addr);
            mem_write(tid, addr, val + 1);
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
            uint32_t *addrj = (uint32_t *)&objs[j];
            uint32_t *addrj1 = (uint32_t *)&objs[j+1];

            uint32_t val = mem_read(tid, addrj);
            mem_write(tid, addrj1, val + 1);

            val = mem_read(tid, addrj1);
            mem_write(tid, addrj, val + 2);
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
            uint32_t *addrj = (uint32_t *)&objs[j];
            uint32_t *addrj1 = (uint32_t *)&objs[j-1];

            uint32_t val = mem_read(tid, addrj);
            mem_write(tid, addrj1, val + 1);

            val = mem_read(tid, addrj1);
            mem_write(tid, addrj, val + 2);
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

    if (posix_memalign((void **)&objs, OBJ_SIZE, NOBJS * OBJ_SIZE) != 0) {
        printf("memory allocation for objs failed\n");
        exit(1);
    }
    obj_id = obj_id_addcnt;

    signal(SIGSEGV, handler);

    nthr = atoi(argv[1]);
    pthread_t *thr;
    thr = calloc_check(nthr, sizeof(pthread_t), "pthread_t array thr");

    // Initialize memory order recorder/replayer
    mem_init((tid_t)nthr);

    for (long i = 0; i < nthr; i++) {
        if (pthread_create(&thr[i], NULL, thr_fn[i % sizeof(thr_fn)/sizeof(thr_fn[0])], (void *)i) != 0) {
            printf("thread creation failed\n");
            exit(1);
        }
    }

    for (long i = 0; i < nthr; i++) {
        pthread_join(thr[i], NULL);
    }

    print_objs();
    return 0;
}
