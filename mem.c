#include "mem.h"
#include <stdlib.h>
#include <stdio.h>

char *objs;

static void __constructor__ init() {
    if (posix_memalign((void **)&objs, OBJ_SIZE, NOBJS * OBJ_SIZE) != 0) {
        printf("memory allocation for objs failed\n");
        exit(1);
    }
}

void print_objs(void) {
    for (int j = 0; j < NOBJS; j++) {
        int64_t *addr = (int64_t *)(objs + j * OBJ_SIZE);
        printf("%lx\n", (long)*addr);
    }
}

