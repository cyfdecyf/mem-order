#include "mem.h"
#include "log.h"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <vector>
#include <iterator>
using namespace std;

enum { BUFFER_ELEMENT_N = LOG_BUFFER_SIZE / (sizeof(int) * 3) };

struct WaitMemop {
    int objid;
    int version;
    int memop;
    WaitMemop(int id, int v, int m) : objid(id), version(v), memop(m) {}
} __attribute__((packed));

typedef vector< vector<WaitMemop> > WaitMemopAll;

long load_wait_memop_log(WaitMemopAll &all, int tid) {
    MappedLog log;
    open_mapped_log("log/memop", tid, &log);

    DPRINTF("map log done, buf start %p end %p\n", log.buf, log.end);

    int *next = (int *)log.buf;
    int *buffer_end = (int *)((long)next + LOG_BUFFER_SIZE);
    int objid, version, memop;
    long total = 0;

    while (*next != -1) {
        objid = *next++;
        version = *next++;
        memop = *next++;

        // No previous memop, no need to wait.
        if (memop == -1) {
            goto skip_padding;
        }
        // printf("%d %d %d\n", objid, version, memop);

        assert(objid < NOBJS);
        all[objid].push_back(WaitMemop(objid, version, memop));
        total++;

skip_padding:
        // Jump over buffer padding
        if ((int *)((long)next + sizeof(WaitMemop)) > buffer_end) {
            next = buffer_end;
            buffer_end = (int *)((long)next + LOG_BUFFER_SIZE);
        }
    }
    unmap_log(log.buf, log.end - log.buf);

    DPRINTF("log loaded\n");
    return total;
}

void write_out_memop_log(const WaitMemopAll &all, long total, int tid) {
    char path[MAX_PATH_LEN];
    logpath(path, "log/sorted-memop", tid);

    int fd = open(path, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    if (fd == -1) {
        perror("creat");
        exit(1);
    }

    long size = total * sizeof(int) * 3;
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate");
        exit(1);
    }

    DPRINTF("Open sorted log done\n");

    WaitMemop *buf = (WaitMemop *)mmap(0, size, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);

    WaitMemopAll::const_iterator objit;
    for (objit = all.begin(); objit != all.end(); ++objit) {
#ifdef DEBUG
        int prev_version = -1;
        for (vector<WaitMemop>::const_iterator it = objit->begin();
            it != objit->end(); ++it) {
            assert(it->version >= prev_version);
            prev_version = it->version;
        }
#endif
        const WaitMemop *st = &(*objit)[0];
        memcpy(buf, st, sizeof(WaitMemop) * objit->size());
        buf += objit->size();
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: processlog <tid>\n");
        exit(1);
    }

    assert(sizeof(WaitMemop) == 3 * sizeof(int));

    int tid;
    istringstream tids(argv[1]);
    tids >> tid;

    WaitMemopAll all(NOBJS);
    long total = load_wait_memop_log(all, tid);
    write_out_memop_log(all, total, tid);

    return 0;
}
