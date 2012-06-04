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

#define DEBUG
#include "debug.h"

struct WaitMemop {
	int objid;
    int version;
    int memop;
    WaitMemop(int id, int v, int m) : objid(id), version(v), memop(m) {}
    WaitMemop() {}
} __attribute__((packed));

struct QueEnt {
	int tid;
	WaitMemop wop;
	QueEnt(int t, const WaitMemop &w) : tid(t), wop(w) {} 

	bool operator>(const QueEnt &rhs) const {
		if (wop.objid == rhs.wop.objid) {
			return wop.version > rhs.wop.version;
		} else {
			return wop.objid > rhs.wop.objid;
		}
	}
};

typedef priority_queue<QueEnt, vector<QueEnt>, greater<QueEnt> > LogQueue;

static inline void enqueue_next_waitmemop(LogQueue &pq, MappedLog &log, WaitMemop &wop, int tid) {
	if (log.buf < log.end) {
		memcpy(&wop, log.buf, sizeof(WaitMemop));
		log.buf += sizeof(WaitMemop);
		pq.push(QueEnt(tid, wop));
	}
}

static void merge_memop(vector<MappedLog> &log, int nthr) {
	LogQueue pq;

	unsigned long total_size = 0;
	for (int i = 0; i < nthr; ++i) {
		struct stat sb;
		if (fstat(log[i].fd, &sb) == -1) {
			perror("fstat in enlarge_mapped_log");
			exit(1);
		}
		total_size += sb.st_size;
	}

	assert(total_size % sizeof(WaitMemop) == 0);
	// we need to add a tid record to each log entry
	total_size += total_size / sizeof(WaitMemop) * sizeof(int); 
	char *outbuf = (char *)create_mapped_file("log/memop", total_size);

	// index buf contains index for an object's log and log entry count
	int *indexbuf = (int *)create_mapped_file("log/memop-index", NOBJS * sizeof(int) * 2);

    DPRINTF("Open sorted log done\n");

	WaitMemop wop;
	for (int i = 0; i < nthr; ++i) {
		enqueue_next_waitmemop(pq, log[i], wop, i);
		DPRINTF("Init T%d %d %d %d\n", i, wop.objid, wop.version, wop.memop);
	}
    DPRINTF("Open sorted log done\n");

    int prev_id = -1, cnt = 0, prev_cnt = 0;;
	while (! pq.empty()) {
		QueEnt qe = pq.top();
		pq.pop();

#ifdef DEBUG
		static int prev_version = -1;
		// DPRINTF("T%d %d %d %d\n", qe.tid, qe.wop.objid, qe.wop.version, qe.wop.memop);
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
				*indexbuf++ = cnt - prev_cnt;
			}
			// Write index as -1 for objid in the range of (previd + 1, curid - 1)
			for (int i = prev_id + 1; i < qe.wop.objid; ++i) {
				DPRINTF("index for obj %d is %d\n", i, -1);
				*indexbuf++ = -1; // index
				*indexbuf++ = 0; // size
			}
			// Write out current object's index
			*indexbuf++ = cnt;
			prev_cnt = cnt;
			prev_id = qe.wop.objid;
			DPRINTF("index for obj %d is %d\n", qe.wop.objid, cnt);
		}

		memcpy(outbuf, &qe.wop, sizeof(WaitMemop));
		*(int *)(outbuf + sizeof(WaitMemop)) = qe.tid;
		outbuf += sizeof(WaitMemop) + sizeof(int);

		enqueue_next_waitmemop(pq, log[qe.tid], wop, qe.tid);	
		cnt++;
	}
	*indexbuf = cnt - prev_cnt;
	DPRINTF("total %d\n", cnt);
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf("Usage: merge-memop <nthr>\n");
        exit(1);
    }

    assert(sizeof(WaitMemop) == 3 * sizeof(int));

    int nthr;
    istringstream nthrs(argv[1]);
    nthrs >> nthr;

    vector<MappedLog> log(nthr);
    for (int i = 0; i < nthr; ++i) {
    	open_mapped_log("log/sorted-memop", i, &log[i]);
    }
    merge_memop(log, nthr);

	return 0;
}