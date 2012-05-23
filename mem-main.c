#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define NITER 30

static int nthr;
static volatile int32_t wflag;

static void wait_other_thread() {
    __sync_fetch_and_add(&wflag, 1);
    while (wflag != nthr)
        asm volatile ("pause");
}

static void *access_thr_fn(void *dummyid) {
    int tid = (int)(long)dummyid;
    mem_init_thr(tid);

    wait_other_thread();

    for (int i = 0; i < NITER; i++) {
        for (int j = 0; j < NOBJS; j++) {
            // Access a 32bit int inside a 64bit int
            int32_t *addr = (int32_t *)&objs[j];
            // Different threads access different part of the shared object
            addr += tid & 1;
            int32_t val = mem_read(addr);
            mem_write(addr, val + 1);
        }
    }
    return NULL;
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <no of threads>\n", argv[0]);
        exit(1);
    }

    nthr = atoi(argv[1]);
    pthread_t *thr;
    thr = calloc_check(nthr, sizeof(pthread_t), "pthread_t array thr");

    // Initialize memory order recorder
    mem_init(nthr);

    for (long i = 0; i < nthr; i++) {
        if (pthread_create(&thr[i], NULL, access_thr_fn, (void *)i) != 0) {
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
