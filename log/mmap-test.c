#include <stdio.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int mapped_write(int fd, off_t size) {
    char *buf;
 
    buf = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
 
    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    memset(buf, 1, size);
 
    if (munmap(buf, size) == -1) {
        perror("munmap");
        return 1;
    }
    return 0;
}

int direct_write(int fd, off_t size) {
    char *buf = malloc(size);

    memset(buf, 1, size);
    if (write(fd, buf, size) != size) {
        perror("write");
        return 1;
    }

    if (close(fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
 
int main(int argc, char *argv[]) {
    int fd = open("./file", O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        return 1;
    }
    printf("size %lld\n", sb.st_size);
    // return mapped_write(fd, sb.st_size);
    return direct_write(fd, sb.st_size);
}