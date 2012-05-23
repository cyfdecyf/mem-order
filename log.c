#include "log.h"
#include <stdlib.h>

#define MAX_PATH_LEN 256

static FILE *handle_log(const char *name, long id, const char *mode) {
    char path[MAX_PATH_LEN];

    if (snprintf(path, MAX_PATH_LEN, "%s-%ld", name, id) >= MAX_PATH_LEN) {
        printf("Path name too long\n");
        exit(1);
    }

    FILE *log = fopen(path, mode);
    if (!log) {
        perror("File open failed");
        exit(1);
    }
    return log;
}

FILE *new_log(const char *name, long id) {
    return handle_log(name, id, "w");
}

FILE *open_log(const char *name, long id) {
    return handle_log(name, id, "r");
}
