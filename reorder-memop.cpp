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

typedef vector< vector<struct wait_memop> > wait_memop_all;

static long load_wait_memop_log(wait_memop_all &all, tid_t tid) {
    struct mapped_log log;
    open_mapped_log("memop", tid, &log);

    DPRINTF("map log done, buf start %p end %p\n", log.buf, log.end);

    struct wait_memop *wmlog = (struct wait_memop *)log.buf;
    long cnt = 0; // cnt is the number of recorded log entries
    long total = 0; // total excludes those with memop -1

    while (wmlog->objid != (objid_t)-1) {
        cnt++;
        // No previous memop, no need to wait.
        if (wmlog->memop == -1) {
            goto skip;
        }
        // printf("%d %d %d\n", objid, version, memop);

        if (wmlog->objid < 0 || wmlog->objid >= g_nobj) {
            printf("ERROR: #%ld objid %d, g_nobj %d\n", cnt, wmlog->objid, g_nobj);
            assert(0);
        }
        // if (wmlog->memop > NITER * g_nobj * 2) {
        //     printf("ERROR: #%ld memop %d > maximum possible %d\n", cnt, (int)wmlog->memop,
        //         NITER * g_nobj * 2);
        //     assert(0);
        // }
        all[wmlog->objid].push_back(*wmlog);
        total++;

skip:
        ++wmlog;
    }
    unmap_log(&log);

    DPRINTF("log loaded, total %ld\n", total);
    return total;
}

static void write_out_memop_log(const wait_memop_all &all, long total, tid_t tid) {
    char path[MAX_PATH_LEN];
    logpath(path, "sorted-memop", tid);

    struct wait_memop *buf = (struct wait_memop *)create_mapped_file(path, total * sizeof(struct wait_memop));
    DPRINTF("Open sorted log done\n");

    wait_memop_all::const_iterator objit;
    for (objit = all.begin(); objit != all.end(); ++objit) {
        const struct wait_memop *st = &(*objit)[0];
        memcpy(buf, st, sizeof(struct wait_memop) * objit->size());
        buf += objit->size();
    }
}

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("Usage: reorder-memop <nobj> <tid>\n");
        exit(1);
    }

    istringstream nobjs(argv[1]);
    nobjs >> g_nobj;
    int tid;
    istringstream nthrs(argv[2]);
    nthrs >> tid;

    wait_memop_all all(g_nobj);
    long total = load_wait_memop_log(all, tid);
    if (total != 0)
        write_out_memop_log(all, total, tid);

    return 0;
}
