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

// #define DEBUG
#include "debug.h"

typedef vector< vector<WaitMemop> > WaitMemopAll;

static long load_wait_memop_log(WaitMemopAll &all, tid_t tid) {
    MappedLog log;
    open_mapped_log("log/memop", tid, &log);

    DPRINTF("map log done, buf start %p end %p\n", log.buf, log.end);

    WaitMemop *next = (WaitMemop *)log.buf;
    WaitMemop *buffer_end = (WaitMemop *)((long)next + LOG_BUFFER_SIZE);
    long total = 0;

    while (next->objid != (objid_t)-1) {
        // No previous memop, no need to wait.
        if (next->memop == -1) {
            goto skip_padding;
        }
        // printf("%d %d %d\n", objid, version, memop);

        if (next->objid >= NOBJS) {
            printf("ERROR: objid %d > NOBJS %d\n", next->objid, NOBJS);
            assert(0);
        }
        all[next->objid].push_back(*next);
        total++;

skip_padding:
        // Jump over buffer padding
        if (++next > buffer_end) {
            next = buffer_end;
            buffer_end = (WaitMemop *)((long)next + LOG_BUFFER_SIZE);
        }
    }
    unmap_log(log.buf, log.end - log.buf);

    DPRINTF("log loaded, total %ld\n", total);
    return total;
}

static void write_out_memop_log(const WaitMemopAll &all, long total, tid_t tid) {
    char path[MAX_PATH_LEN];
    logpath(path, "log/sorted-memop", tid);

    WaitMemop *buf = (WaitMemop *)create_mapped_file(path, total * sizeof(WaitMemop));
    DPRINTF("Open sorted log done\n");

    WaitMemopAll::const_iterator objit;
    for (objit = all.begin(); objit != all.end(); ++objit) {
        const WaitMemop *st = &(*objit)[0];
        memcpy(buf, st, sizeof(WaitMemop) * objit->size());
        buf += objit->size();
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: reorder-memop <tid>\n");
        exit(1);
    }

    tid_t tid;
    istringstream tids(argv[1]);
    tids >> tid;

    WaitMemopAll all(NOBJS);
    long total = load_wait_memop_log(all, tid);
    if (total != 0)
        write_out_memop_log(all, total, tid);

    return 0;
}
