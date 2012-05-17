#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <vector>
#include <deque>
#include <cassert>
using namespace std;

const string RDLOG_PATTERN = "rec-rd";
const string WRLOG_PATTERN = "rec-wr";
const string WARLOG_PATTERN = "war-rd";

struct ReadLogEnt {
	int read_memop;
	int objid;
	int version;
	int last_read_memop;
};

istream& operator>>(istream &is, ReadLogEnt &rhs) {
	is >> rhs.read_memop >> rhs.objid >>  rhs.version >> rhs.last_read_memop;
	return is;
}

ostream& operator<<(ostream &os, ReadLogEnt &rhs) {
	os << "read_memop: " << rhs.read_memop
	   << " objid: " << rhs.objid
	   << " version: " << rhs.version
	   << " last_read_memop: " << rhs.last_read_memop;
	return os;
}

struct ReadLog {
	int nthr;
	int tid;
	vector<int> prev_version; // Each object has a previsou read version

	ReadLogEnt log_ent;
	ifstream readlog;

	ReadLog(int nthr, int tid, int nobj) : nthr(nthr), tid(tid), prev_version(nobj) {
		// Open read log
		ostringstream logpath;
		logpath << RDLOG_PATTERN << "-" << tid;
		readlog.open(logpath.str().c_str());
		if (! readlog) {
			cerr << "Read log " << logpath.str() << " open failed" << endl;
			exit(1);
		}
	}

	void infer_write_after_read() {
		ostringstream warlogpath;
		warlogpath << WARLOG_PATTERN << "-" << tid;
		ofstream warlog(warlogpath.str().c_str());
		if (! warlog) {
			cerr << "Can't create WAL log " << warlogpath.str() << endl;
			exit(1);
		}

		while (readlog >> log_ent) {
			// For each read log entry
			// 1. the previous read operation must has a different version with the log
			// entry.
			// So the write which modifies at that version must wait the previous read.
			// We don't know which thread did that write, but as write forms a total
			// order, we can just dump out log.

			// Log format: objid prev_version tid last_memop
			assert(prev_version[log_ent.objid] != log_ent.version);
			warlog << log_ent.objid << " "
				   << prev_version[log_ent.objid] << " "
				   << tid << " "
				   << log_ent.last_read_memop << endl;
		}
	}
};

int main(int argc, char const *argv[]) {
	if (argc != 4) {
		cout << "Usage: " << argv[0] << " <nthread> <tid> <nobj>" << endl;
		exit(1);
	}

	int nthr, tid, nobj;

	istringstream nthr_ss(argv[1]);
	nthr_ss >> nthr;
	istringstream tid_ss(argv[2]);
	tid_ss >> tid;
	istringstream nobj_ss(argv[3]);
	nobj_ss >> nobj;

	ReadLog rl(nthr, tid, nobj);
	rl.infer_write_after_read();

	return 0;
}