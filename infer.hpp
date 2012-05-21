#ifndef __INFER_CPP
#define __INFER_CPP

#include <vector>
#include <deque>
#include <iterator>
#include <fstream>

struct ReadLogEnt {
	int read_memop;
	int objid;
	int version;
	int last_read_memop;
};

std::istream& operator>>(std::istream &is, ReadLogEnt &rhs);
std::ostream& operator<<(std::ostream &os, ReadLogEnt &rhs);

struct WriteLogEnt {
	int objid;
	int version;
};

std::istream& operator>>(std::istream &is, WriteLogEnt &rhs);
std::ostream& operator<<(std::ostream &os, WriteLogEnt &rhs);

class VersionMemop {
public:
	int version;
	int memop;

	VersionMemop(int ver, int mop) : version(ver), memop(mop) {}
};

class ReadLog {
	std::vector<int> last_read_version; // Each object has a last read version
	std::vector< std::deque<VersionMemop> > version_memop;
	std::ifstream readlog;
	// For debug
	std::vector<int> prev_query_version;

public:
	ReadLog(int nobj, const char *logpath = NULL);

	void openlog(const char *logpath);

	// Search the last read memop that get value @version.
	// This read memop is used to generate write-after-read log.	
	// Return true if found.
	bool read_at_version_on_obj(int version, int read_objid, int &result_memop);
};

struct VersionRange {
	int low;
	int high;
};

class WriteLog {
	std::vector< std::deque<int> > last_write_version;
	std::ifstream writelog;

public:
	WriteLog(int nobj, const char *logpath = NULL);

	// Return false if no more write log.
	bool next_write_version(int objid, int &version);

	void openlog(const char *logpath);
};

class Infer {
	int tid;
	std::vector<WriteLog *> wlog;
	std::vector<ReadLog *> rlog;
	std::ofstream war_out;

public:
	Infer(int tid, int nthr, int nobj, const char *logdir);

	void infer();
};

#endif