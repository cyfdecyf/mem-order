#include "mem.h"
#include "log.h"
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <queue>
#include <vector>
#include <sstream>
using namespace std;

// #define DEBUG
#include "debug.h"

struct q_ent {
    tid_t tid;
    uint64_t ts;
    
    q_ent(tid_t t, const uint64_t ts) : tid(t), ts(ts) {} 

    bool operator>(const q_ent &rv) const {
        return ts > rv.ts;
    }
};

uint64_t *next_ts(struct mapped_log *tslog) {
    return (uint64_t *)read_log_entry(tslog, sizeof(uint64_t));
}

static void merge_commit(vector<struct mapped_log> &log, tid_t nthr) {
    assert((int)log.size() == nthr);

    struct mapped_log commitlog;
    if (new_mapped_log_path(LOGDIR"commit", &commitlog) == -1) {
        perror("create commit order log");
        exit(1);
    }

    priority_queue<q_ent, vector<q_ent>, greater<q_ent> > ts_q;

    for (unsigned int i = 0; i < log.size(); i++) {
        if (log[i].fd == -1) continue;

        uint64_t *ts = next_ts(&log[i]);
        if (ts != NULL) {
            ts_q.push(q_ent((tid_t)i, *ts));
            //printf("T%u %ld\n", i, *ts);
        }
    }

    while (!ts_q.empty()) {
        q_ent e = ts_q.top();
        ts_q.pop();

        //printf("T%d %ld\n", e.tid, e.ts);
        tid_t *p = (tid_t *)next_log_entry(&commitlog, sizeof(tid_t));
        *p = e.tid;

        // Enqueue next ts.
        uint64_t *nts = next_ts(&log[e.tid]);
        if (nts != NULL) {
            ts_q.push(q_ent(e.tid, *nts));
        }
    }

    unmap_truncate_log(&commitlog);
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: merge-tsc <nthr>\n");
        exit(1);
    }

    int nthr;
    istringstream nthrs(argv[1]);
    nthrs >> nthr;

    vector<struct mapped_log> log;
    struct mapped_log l;
    for (int i = 0; i < nthr; ++i) {
        // XXX Push the log structure into the vector even if open failed,
        // because the index in the vector represents thread id.
        open_mapped_log("commit", i, &l);
        log.push_back(l);
    }
    merge_commit(log, (tid_t)nthr);

    return 0;
}
