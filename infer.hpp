#ifndef __INFER_CPP
#define __INFER_CPP

#include <vector>
#include <deque>
#include <queue>
#include <iterator>
#include <fstream>
#include <functional>

typedef int version_t;
typedef int objid_t;
typedef int memop_t;

struct ReadLogEnt {
	memop_t read_memop;
	objid_t objid;
	version_t version;
	memop_t last_read_memop;
};

std::istream& operator>>(std::istream &is, ReadLogEnt &rhs);
std::ostream& operator<<(std::ostream &os, ReadLogEnt &rhs);

struct WriteLogEnt {
	objid_t objid;
	version_t version;
};

std::istream& operator>>(std::istream &is, WriteLogEnt &rhs);
std::ostream& operator<<(std::ostream &os, WriteLogEnt &rhs);

class VersionMemop {
public:
	version_t version;
	memop_t memop;

	VersionMemop(version_t ver, memop_t mop) : version(ver), memop(mop) {}
};

class ReadLog {
	std::vector<version_t> last_read_version; // Each object has a last read version
	std::vector< std::deque<VersionMemop> > version_memop;
	std::ifstream readlog;
	// For debug
	std::vector<int> prev_query_version;

public:
	ReadLog(objid_t nobj, const char *logpath = NULL);

	void openlog(const char *logpath);

	// Search the last read memop that get value @version.
	// This read memop is used to generate write-after-read log.	
	// Return true if found.
	bool read_at_version_on_obj(version_t version, objid_t read_objid, memop_t &result_memop);
};

class WriteLog {
	std::vector< std::deque<version_t> > last_write_version;
	std::ifstream writelog;

public:
	WriteLog(objid_t nobj, const char *logpath = NULL);

	void openlog(const char *logpath);

	// Return false if no more write log.
	bool next_write_version(objid_t objid, version_t &version);
};

struct TidVersion {
	int tid;
	int version;

public:
	TidVersion(int tid, version_t version) : tid(tid), version(version) {}
	inline bool operator>(const TidVersion rhs) const {
		return version > rhs.version;
	}
};

typedef std::priority_queue<TidVersion,
			std::vector<TidVersion>,
			std::greater<TidVersion> > version_queue;

class Infer {
	int tid;
	int nthr;

	// Store each thread's write log
	std::vector<WriteLog *> wlog;
	// The thread inferring order doesn't need it's read log
	std::vector<ReadLog *> rlog;
	std::ofstream war_out;

	std::vector<version_queue> first_write_version;
	std::vector<bool> first_write_queue_filled;
	void init_first_write_queue(objid_t objid, version_queue &first_write);

	// Find the last "follwing write" after the start version.
	// "following write" are writes that happen before other thread's write
	// to this object.
	// Return the version of the last following writes. If nothing found,
	// return start_version.
	version_t following_write_version(objid_t objid, version_t start_version);

public:
	Infer(int tid, int nthr, objid_t nobj, const char *logdir);

	void infer();
};

#endif