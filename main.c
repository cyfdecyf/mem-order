#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define NITER 10000

int nthr;
static volatile int32_t wflag;

static void wait_other_thread() {
    __sync_fetch_and_add(&wflag, 1);
    while (wflag != nthr) {
        asm volatile ("pause");
    }
}

void *access_thr_fn(void *dummyid) {
    long thr_id = (long)dummyid;

    wait_other_thread();

    for (int i = 0; i < NITER; i++) {
        for (int j = 0; j < NOBJS; j++) {
            int32_t *addr = (int32_t *)(objs + j * OBJ_SIZE);
            // Different threads access different part of the shared object
            addr += thr_id & 1;
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
    thr = calloc(nthr, sizeof(pthread_t));

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
