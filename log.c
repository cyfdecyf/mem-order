#include "log.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

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

void *open_mapped_log(const char *name, int id, int *pfd) {
    char path[MAX_PATH_LEN];
    logpath(path, name, id);

    int fd = open(path, O_RDWR|O_CREAT|O_TRUNC,
        S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd == -1) {
        perror("creat");
        exit(1);
    }
    if (pfd)
        *pfd = fd;
#ifdef DEBUG
    fprintf(stderr, "%s fd %d\n", path, fd);
#endif

    if (ftruncate(fd, LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }
    assert(sb.st_size == LOG_BUFFER_SIZE);
    char *buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE, MAP_SHARED,
        fd, 0);
    if (buf == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    if (madvise(buf, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise");
        exit(1);
    }
    return buf;
}

pthread_mutex_t map_lock = PTHREAD_MUTEX_INITIALIZER;

void *enlarge_mapped_log(void *buf, int fd) {
    pthread_mutex_lock(&map_lock);
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }
    off_t original_size = sb.st_size;
    
#ifdef DEBUG
    fprintf(stderr, "fd %d unmap %p ", fd, buf);
#endif
    if (munmap(buf, LOG_BUFFER_SIZE) == -1) {
        perror("munmap");
        exit(1);
    }
    
    if (ftruncate(fd, original_size + LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE, MAP_SHARED, fd, original_size);
    if (buf == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
#ifdef DEBUG
    fprintf(stderr, "new buf: %p truncate to %lld bytes\n", buf,
        (long long int)(original_size + LOG_BUFFER_SIZE));
#endif

    if (madvise(buf, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise");
        exit(1);
    }
    pthread_mutex_unlock(&map_lock);
    return buf;
}