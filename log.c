#include "log.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

// #define DEBUG

#define PAGE_SIZE 4096

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

int new_mapped_log(const char *name, int id, struct mapped_log *log) {
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

    log->start = log->buf = mmap(0, LOG_BUFFER_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, log->fd, 0);
    if (log->buf == MAP_FAILED) {
        perror("mmap in new_mapped_log");
        exit(1);
    }
    log->end = log->start + LOG_BUFFER_SIZE;
    if (madvise(log->start, LOG_BUFFER_SIZE, MADV_SEQUENTIAL) == -1) {
        perror("madvise in new_mapped_log");
        exit(1);
    }
    return 0;
}

int enlarge_mapped_log(struct mapped_log *log) {
    struct stat sb;
    if (fstat(log->fd, &sb) == -1) {
        perror("fstat in enlarge_mapped_log");
        exit(1);
    }
    off_t original_size = sb.st_size;
    assert(original_size % LOG_BUFFER_SIZE == 0);
    
#ifdef DEBUG
    fprintf(stderr, "fd %d unmap start %p buf %p ", log->fd, log->start, log->buf);
#endif
    if (munmap(log->start, log->end - log->start) == -1) {
        perror("munmap in enlarge_mapped_log");
        exit(1);
    }
    
    if (ftruncate(log->fd, original_size + LOG_BUFFER_SIZE) == -1) {
        perror("ftruncate in enlarge_mapped_log");
        exit(1);
    }

    int map_size = LOG_BUFFER_SIZE + PAGE_SIZE;
    log->start = mmap(0, map_size, PROT_WRITE|PROT_READ, MAP_SHARED,
        log->fd, original_size - PAGE_SIZE);
    if (log->start == MAP_FAILED) {
        perror("mmap in enlarge_mapped_log");
        exit(1);
    }
    log->end = log->start + map_size;
    long page_offset = (long)log->buf & 0xFFF;
    // If page offset is 0, means we need to start on the new page.
    log->buf = log->start + (page_offset ? page_offset : PAGE_SIZE);

#ifdef DEBUG
    fprintf(stderr, "new start: %p buf: %p truncate to %ld bytes\n", log->start,
        log->buf, (long)(original_size + LOG_BUFFER_SIZE));
#endif

    if (madvise(log->start, map_size, MADV_SEQUENTIAL) == -1) {
        perror("madvise in enlarge_mapped_log");
        exit(1);
    }
    return 0;
}

int open_mapped_log_path(const char *path, struct mapped_log *log) {
    log->fd = open(path, O_RDONLY);
    if (log->fd == -1) {
        // Make start, buf and end all the same. This marks that the log is empty.
        log->start = log->end = log->buf = NULL;
        return -1;
    }

    struct stat sb;
    if (fstat(log->fd, &sb) == -1) {
        perror("fstat");
        exit(1);
    }
    log->start = log->buf = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, log->fd, 0);
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

int open_mapped_log(const char *name, int id, struct mapped_log *log) {
    char path[MAX_PATH_LEN];
    logpath(path, name, id);

    return open_mapped_log_path(path, log);
}

int unmap_log(struct mapped_log *log) {
    if (munmap(log->start, log->end - log->start) == -1) {
        perror("munmap");
        return -1;
    }
    return 0;
}

void *create_mapped_file(const char *path, unsigned long size) {
    int fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd == -1) {
        perror("open in create_mapped_file");
        exit(1);
    }

    if (ftruncate(fd, size) == -1) {
        perror("ftruncate in create_mapped_file");
        exit(1);
    }
    void *buf = mmap(0, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        perror("mmap in create_mapped_file");
        exit(1);
    }
    return buf;
}
