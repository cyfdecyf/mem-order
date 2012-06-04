#include "mem.h"
#include "log.h"
#include <cassert>
#include <cstdio>
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

#define DEBUG
#include "debug.h"

enum { BUFFER_ELEMENT_N = LOG_BUFFER_SIZE / (sizeof(int) * 3) };

struct WaitMemop {
    int objid;
    int version;
    int memop;
    WaitMemop(int id, int v, int m) : objid(id), version(v), memop(m) {}
} __attribute__((packed));

typedef vector< vector<WaitMemop> > WaitMemopAll;

static long load_wait_memop_log(WaitMemopAll &all, int tid) {
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

    DPRINTF("log loaded, total %ld\n", total);
    return total;
}

static void write_out_memop_log(const WaitMemopAll &all, long total, int tid) {
    char path[MAX_PATH_LEN];
    logpath(path, "log/sorted-memop", tid);

    WaitMemop *buf = (WaitMemop *)create_mapped_file(path, total * sizeof(WaitMemop));
    DPRINTF("Open sorted log done\n");

    WaitMemopAll::const_iterator objit;
    for (objit = all.begin(); objit != all.end(); ++objit) {
#ifdef DEBUG
        int prev_version = -1, prev_objid = -1;
        for (vector<WaitMemop>::const_iterator it = objit->begin();
            it != objit->end(); ++it) {
            assert(it->version >= prev_version);
            assert(it->objid >= prev_objid);
            prev_version = it->version;
            prev_objid = it->objid;

            *buf = *it;
            ++buf;
        }
#else
        const WaitMemop *st = &(*objit)[0];
        memcpy(buf, st, sizeof(WaitMemop) * objit->size());
        buf += objit->size();
#endif
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: reorder-memop <tid>\n");
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
