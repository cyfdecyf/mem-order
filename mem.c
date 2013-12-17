#include "mem.h"
#include <stdlib.h>
#include <stdio.h>

__thread tid_t tid;

objid_t (*obj_id)(void *addr);

void *calloc_check(long nmemb, long size, const char *err_msg) {
    void *p = calloc(nmemb, size);
    if (!p) {
        printf("memory allocation failed: %s\n", err_msg);
        exit(1);
    }
    return p;
}
