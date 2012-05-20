#ifndef __INFER_CPP
#define __INFER_CPP

#include <vector>
#include <deque>
#include <iterator>
#include <fstream>

#define DEBUG

struct ReadLogEnt {
	int read_memop;
	int objid;
	int version;
	int last_read_memop;
};

std::istream& operator>>(std::istream &is, ReadLogEnt &rhs);
std::ostream& operator<<(std::ostream &os, ReadLogEnt &rhs);

struct VerMemop {
	int version;
	int memop;

	VerMemop(int ver, int mop) : version(ver), memop(mop) {}
};

struct ReadLog {
	int nthr;
	int tid;
	std::vector<int> prev_version; // Each object has a previsou read version
	std::vector< std::deque<VerMemop> > version_memop;

	std::ifstream readlog;

	ReadLog(int nthr, int tid, int nobj);

	// Return true if found
	bool read_at_version_on_obj(int version, int read_objid, int &result_memop);

private:
#ifdef DEBUG
	// For debugging. Previous query version to read_at_version_on_obj
	int prev_query_version;
#endif
};

#endif