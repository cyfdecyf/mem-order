#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define NITER 10000

pthread_barrier_t barrier;

void *access_thr_fn(void *dummyid) {
    long thr_id = (long)dummyid;

    pthread_barrier_wait(&barrier);

    for (int i = 0; i < NITER; i++) {
        for (int j = 0; j < NOBJS; j++) {
            int32_t *addr = (int32_t *)(objs + j * OBJ_SIZE);
            // Different threads access different part of the shared object
            addr += thr_id & 1;
            int32_t val = mem_read(addr);
            mem_write(addr, val + 1);
        }
    }
}

int main(int argc, const char *argv[]) {
    int nthr = 0;
    if (argc != 2) {
        printf("Usage: %s <no of threads>\n", argv[0]);
        exit(1);
    }

    nthr = atoi(argv[1]);
    pthread_t *thr;
    thr = calloc(nthr, sizeof(pthread_t));

    if (pthread_barrier_init(&barrier, NULL, nthr) != 0) {
        printf("barrier init failed\n");
        exit(1);
    }

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
