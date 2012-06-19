#include "mem.h"
#include <stdlib.h>
#include <stdio.h>

int64_t *objs;

#ifdef __linux
__thread tid_t tid;
#else
pthread_key_t tid_key;
#endif

static void __constructor__ init() {
    if (posix_memalign((void **)&objs, OBJ_SIZE, NOBJS * OBJ_SIZE) != 0) {
        printf("memory allocation for objs failed\n");
        exit(1);
    }
#ifndef __linux
    if (pthread_key_create(&tid_key, NULL)) {
        printf("thr_id key creation failed\n");
        exit(1);
    }
#endif
}

void print_objs(void) {
    for (int i = 0; i < NOBJS; i++) {
        printf("%lx\n", (long)objs[i]);
    }
}

void *calloc_check(size_t nmemb, size_t size, const char *err_msg) {
    void *p = calloc(nmemb, size);
    if (!p) {
        printf("memory allocation failed: %s\n", err_msg);
        exit(1);
    }
    return p;
}
