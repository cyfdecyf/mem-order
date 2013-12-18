#include "mem.h"
#include <stdlib.h>
#include <stdio.h>

__thread tid_t g_tid;

objid_t (*calc_objid)(void *addr);

void *calloc_check(long nmemb, long size, const char *err_msg) {
    void *p = calloc(nmemb, size);
    if (!p) {
        printf("memory allocation failed: %s\n", err_msg);
        exit(1);
    }
    return p;
}
