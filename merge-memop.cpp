#include "mem.h"
#include "log.h"
#include <cstdio>
#include <sys/stat.h>
#include <cassert>
#include <cstring>
#include <utility>
#include <queue>
#include <vector>
#include <sstream>
using namespace std;

// #define DEBUG
#include "debug.h"

struct QueEnt {
    tid_t tid;
    struct wait_memop wop;
    QueEnt(tid_t t, const struct wait_memop &w) : tid(t), wop(w) {} 

    bool operator>(const QueEnt &rhs) const {
        if (wop.objid == rhs.wop.objid) {
            return wop.version > rhs.wop.version;
        } else {
            return wop.objid > rhs.wop.objid;
        }
    }
};

typedef priority_queue<QueEnt, vector<QueEnt>, greater<QueEnt> > LogQueue;

static inline void enqueue_next_waitmemop(LogQueue &pq, struct mapped_log &log, struct wait_memop &wop, tid_t tid) {
    if (log.buf < log.end) {
        memcpy(&wop, log.buf, sizeof(struct wait_memop));
        log.buf += sizeof(struct wait_memop);
        pq.push(QueEnt(tid, wop));
    }
}

static void merge_memop(vector<struct mapped_log> &log, tid_t nthr) {
    LogQueue pq;

    unsigned long total_size = 0;
    for (int i = 0; i < nthr; ++i) {
        // Only add file size if it's opened correctly
        if (log[i].fd != -1) {
            struct stat sb;
            if (fstat(log[i].fd, &sb) == -1) {
                perror("fstat in enlarge_mapped_log");
                exit(1);
            }
            total_size += sb.st_size;
        }
    }

    if (total_size == 0) {
        exit(1);
    }
    assert(total_size % sizeof(struct wait_memop) == 0);
    // we need to add a tid record to each log entry
    int entrycount = total_size / sizeof(struct wait_memop);
    struct replay_wait_memop *next_mwm = (struct replay_wait_memop *)create_mapped_file(LOGDIR"memop",
        entrycount * sizeof(*next_mwm));
    DPRINTF("created memop log\n");

    // index buf contains index for an object's log and log entry count
    int *indexbuf = (int *)create_mapped_file(LOGDIR"memop-index",
        g_nobj * sizeof(int) * 2);
    DPRINTF("created memop-index log\n");

    struct wait_memop wop;
    for (int i = 0; i < nthr; ++i) {
        if (log[i].fd == -1)
            continue;
        enqueue_next_waitmemop(pq, log[i], wop, i);
        DPRINTF("Init Queue add T%d %d %d %d\n", i, (int)wop.objid, (int)wop.version,
            (int)wop.memop);
    }

    objid_t prev_id = -1;
    int cnt = 0, prev_cnt = 0;;
    while (! pq.empty()) {
        QueEnt qe = pq.top();
        pq.pop();

#ifdef DEBUG
        static version_t prev_version = -1;
        DPRINTF("T%d %d %d %d\n", qe.tid, qe.wop.objid, qe.wop.version, qe.wop.memop);
        assert(qe.wop.objid >= prev_id);
        if (qe.wop.objid != prev_id) {
            prev_version = -1;
        } else {
            assert(qe.wop.version >= prev_version);
        }
        prev_version = qe.wop.version;
#endif

        // Write object index if needed. Previous id has index written.
        if (prev_id != qe.wop.objid) {
            if (prev_id != -1) {
                // Write out previous object's log entry count
                *indexbuf = cnt - prev_cnt;
                // XXX note here. The log contains entry with count as -1j
                assert(*indexbuf > 0);
                DPRINTF("obj %d index %d log entry count %d\n", prev_id, prev_cnt, *indexbuf);
                indexbuf++;
            }
            // Write index as -1 for objid in the range of (previd + 1, curid - 1)
            for (int i = prev_id + 1; i < qe.wop.objid; ++i) {
                DPRINTF("index for obj %d is %d, empty log\n", i, -1);
                *indexbuf++ = -1; // index
                *indexbuf++ = 0; // size
            }
            // Write out current object's index
            *indexbuf++ = cnt;
            prev_cnt = cnt;
            prev_id = qe.wop.objid;
        }

        next_mwm->version = qe.wop.version;
        next_mwm->memop = qe.wop.memop;
        next_mwm->tid = qe.tid;
        next_mwm++;

        // The following code dumps the object id in the log. But with object index,
        // this is not needed. Keep it here because this is useful info for manual inspecting
        // the log.
        /*
        memcpy(outbuf, &qe.wop, sizeof(struct wait_memop));
        *(int *)(outbuf + sizeof(struct wait_memop)) = qe.tid;
        outbuf += sizeof(struct wait_memop) + sizeof(int);
        */

        enqueue_next_waitmemop(pq, log[qe.tid], wop, qe.tid);   
        cnt++;
    }
    // Last object's log size
    *indexbuf++ = cnt - prev_cnt;

    for (int i = prev_id + 1; i < g_nobj; ++i) {
        *indexbuf++ = -1;
        *indexbuf++ = 0;
    }
    DPRINTF("obj %d index %d log entry count %d\n", prev_id, prev_cnt, *indexbuf);
    DPRINTF("total #wait_memop %d\n", cnt);
}

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("Usage: merge-memop <nthr> <nobj>\n");
        exit(1);
    }

    int nthr;
    istringstream nthrs(argv[1]);
    nthrs >> nthr;
    istringstream nobjs(argv[2]);
    nobjs >> g_nobj;

    vector<struct mapped_log> log;
    struct mapped_log l;
    for (int i = 0; i < nthr; ++i) {
        // Push the log structure into the vector even if open failed
        open_mapped_log("sorted-memop", i, &l);
        log.push_back(l);
    }
    merge_memop(log, (tid_t)nthr);

    return 0;
}
