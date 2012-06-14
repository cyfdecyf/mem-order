#include "log.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

// #define DEBUG

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
        perror("creat in new_mapped_log");
        exit(1);
    }
#ifdef DEBUG
    fprintf(stderr, "%s fd %d\n", path, log->fd);
#endif

    if (ftruncate(log->fd, LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate in new_mapped_log");
        exit(1);
    }

    log->buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, log->fd, 0);
    if (log->buf == MAP_FAILED) {
        perror("mmap in new_mapped_log");
        exit(1);
    }
    log->end = log->buf + LOG_BUFFER_SIZE;
    if (madvise(log->buf, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise in new_mapped_log");
        exit(1);
    }
    return 0;
}

int enlarge_mapped_log(MappedLog *log) {
    struct stat sb;
    if (fstat(log->fd, &sb) == -1) {
        perror("fstat in enlarge_mapped_log");
        exit(1);
    }
    off_t original_size = sb.st_size;
    assert(original_size % LOG_BUFFER_SIZE == 0);
    
#ifdef DEBUG
    fprintf(stderr, "fd %d unmap %p ", log->fd, log->buf);
#endif
    if (munmap(log->end - LOG_BUFFER_SIZE, LOG_BUFFER_SIZE) == -1) {
        perror("munmap in enlarge_mapped_log");
        exit(1);
    }
    
    if (ftruncate(log->fd, original_size + LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate in enlarge_mapped_log");
        exit(1);
    }

    log->buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED,
        log->fd, original_size);
    if (log->buf == MAP_FAILED) {
        perror("mmap in enlarge_mapped_log");
        exit(1);
    }
    log->end = log->buf + LOG_BUFFER_SIZE;
    assert(*((int *)log->buf) == 0);
#ifdef DEBUG
    fprintf(stderr, "new buf: %p truncate to %lld bytes\n", log->buf,
        (long long int)(original_size + LOG_BUFFER_SIZE));
#endif

    if (madvise(log->buf, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise in enlarge_mapped_log");
        exit(1);
    }
    return 0;
}

int open_mapped_log_path(const char *path, MappedLog *log) {
    log->fd = open(path, O_RDONLY);
    if (log->fd == -1) {
        fprintf(stderr, "Can't open file %s\n", path);
        exit(1);
    }

    struct stat sb;
    if (fstat(log->fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }
    log->buf = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, log->fd, 0);
    if (log->buf == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    log->end = log->buf + sb.st_size;
    if (madvise(log->buf, sb.st_size, MADV_SEQUENTIAL) == -1) {
        perror("madvise");
        exit(1);
    }
    return 0;
}

int open_mapped_log(const char *name, int id, MappedLog *log) {
    char path[MAX_PATH_LEN];
    logpath(path, name, id);

    return open_mapped_log_path(path, log);
}

int unmap_log(void *start, off_t size) {
    if (munmap(start, size) == -1) {
        perror("munmap");
        return -1;
    }
    return 0;
}

void *create_mapped_file(const char *path, unsigned long size) {
    int fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd == -1) {
        perror("open in map_fixed_size_file");
        exit(1);
    }

    if (ftruncate(fd, size) == -1) {
        perror("ftruncate in map_fixed_size_file");
        exit(1);
    }
    void *buf = mmap(0, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        perror("mmap in map_fixed_size_file");
        exit(1);
    }
    return buf;
}
