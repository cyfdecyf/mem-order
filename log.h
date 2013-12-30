#ifndef _LOG_H
#define _LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>

FILE *new_log(const char *name, long id);
FILE *open_log(const char *name, long id);

// enum { LOG_BUFFER_SIZE = 4 * 1024 * 1024 }; // For testing, make it small to make enlarge necessary.
enum { LOG_BUFFER_SIZE = 4 * 1024 }; // For testing, make it small to make enlarge necessary.

struct mapped_log {
    char *start;
    char *buf; // buf can be changed
    char *end;
    int fd;
};

int new_mapped_log(const char *name, int id, struct mapped_log *log);
int enlarge_mapped_log(struct mapped_log *log);

// The following two function return 0 on success.
int open_mapped_log(const char *name, int id, struct mapped_log *log);
int open_mapped_log_path(const char *path, struct mapped_log *log);

int truncate_log(struct mapped_log *log);
int unmap_log(struct mapped_log *log);
int unmap_truncate_log(struct mapped_log *log);

void *create_mapped_file(const char *name, unsigned long size);

static inline char *next_log_entry(struct mapped_log *log, int entry_size) {
    if ((log->buf + entry_size) > log->end) {
        enlarge_mapped_log(log);
    }
    char *start = log->buf;
    log->buf += entry_size;
    return start;
}

static inline char *read_log_entry(struct mapped_log *log, int entry_size) {
    if ((log->buf + entry_size) > log->end) {
        printf("no more log entry");
        return NULL;
    }
    char *start = log->buf;
    log->buf += entry_size;
    return start;
}

static inline int log_end(struct mapped_log *log) {
    return log->buf >= log->end;
}

#define LOGDIR "replay-log/"

#define MAX_PATH_LEN 256
static inline void logpath(char *buf, const char *name, long id) {
    if (snprintf(buf, MAX_PATH_LEN, LOGDIR"%s-%ld", name, id) >= MAX_PATH_LEN) {
        printf("Path name too long\n");
        exit(1);
    }
}

#ifdef __cplusplus
}
#endif

#endif
