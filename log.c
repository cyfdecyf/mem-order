#include "log.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

// #define DEBUG

#define MAX_PATH_LEN 256

static inline void logpath(char *buf, const char *name, long id) {
    if (snprintf(buf, MAX_PATH_LEN, "%s-%ld", name, id) >= MAX_PATH_LEN) {
        printf("Path name too long\n");
        exit(1);
    }
}

static FILE *handle_log(const char *name, long id, const char *mode) {
    char path[MAX_PATH_LEN];
    logpath(path, name, id);

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

int new_mapped_log(const char *name, int id, MappedLog *log) {
    char path[MAX_PATH_LEN];
    logpath(path, name, id);

    log->fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (log->fd == -1) {
        perror("creat");
        exit(1);
    }
#ifdef DEBUG
    fprintf(stderr, "%s fd %d\n", path, log->fd);
#endif

    if (ftruncate(log->fd, LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    struct stat sb;
    if (fstat(log->fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }
    assert(sb.st_size == LOG_BUFFER_SIZE);
    log->buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, log->fd, 0);
    if (log->buf == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    log->end = log->buf + LOG_BUFFER_SIZE;
    if (madvise(log->buf, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise");
        exit(1);
    }
    return 0;
}

int enlarge_mapped_log(MappedLog *log) {
    struct stat sb;
    if (fstat(log->fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }
    off_t original_size = sb.st_size;
    
#ifdef DEBUG
    fprintf(stderr, "fd %d unmap %p ", fd, buf);
#endif
    if (munmap(log->end - LOG_BUFFER_SIZE, LOG_BUFFER_SIZE) == -1) {
        perror("munmap");
        exit(1);
    }
    
    if (ftruncate(log->fd, original_size + LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    log->buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED,
        log->fd, original_size);
    if (log->buf == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    log->end = log->buf + LOG_BUFFER_SIZE;
#ifdef DEBUG
    fprintf(stderr, "new buf: %p truncate to %lld bytes\n", buf,
        (long long int)(original_size + LOG_BUFFER_SIZE));
#endif

    if (madvise(log->buf, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise");
        exit(1);
    }
    return 0;
}
