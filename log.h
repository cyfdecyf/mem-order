#ifndef _LOG_H
#define _LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

FILE *new_log(const char *name, long id);
FILE *open_log(const char *name, long id);

enum { LOG_BUFFER_SIZE = 4 * 1024 * 1024 }; // For testing, make it small to make enlarge necessary.

typedef struct {
    char *buf;
    char *end;
    int fd;
} MappedLog;

int new_mapped_log(const char *name, int id, MappedLog *log);
int enlarge_mapped_log(MappedLog *log);

static inline char *next_log_start(MappedLog *log, int entry_size) {
    if ((log->buf + entry_size) > log->end) {
        enlarge_mapped_log(log);
    }
    char *start = log->buf;
    log->buf += entry_size;
    return start;
}

#ifdef __cplusplus
}
#endif

#endif
