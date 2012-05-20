#include "infer.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cassert>
using namespace std;

const string RDLOG_PATTERN = "rec-rd";
const string WRLOG_PATTERN = "rec-wr";
const string WARLOG_PATTERN = "war-rd";

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

ReadLog::ReadLog(int nthr, int tid, int nobj) : nthr(nthr), tid(tid),
	prev_version(nobj), version_memop(nobj),
	prev_query_version(-1) {
	// Open read log
	ostringstream logpath;
	logpath << RDLOG_PATTERN << "-" << tid;
	readlog.open(logpath.str().c_str());
	if (! readlog) {
		cerr << "Read log " << logpath.str() << " open failed" << endl;
		exit(1);
	}
}

bool ReadLog::read_at_version_on_obj(int version, int read_objid, int &result_memop) {
	// Because read and write can only get version that's larger then
	// previous one, the version parameter given to this function
	// should always increase
#ifdef DEBUG
	assert(version > prev_query_version);
	prev_query_version = version;
#endif

	// Search in already read version and memop info
	deque<VerMemop> &vermemop = version_memop[read_objid];
	for (deque<VerMemop>::iterator it = vermemop.begin();
		it != vermemop.end();
		++it) {
		if (it->version > version)
			return false;

		vermemop.pop_front();
		if (it->version == version) {
			result_memop = it->memop;
			return true;
		}
	}

	// Search in the read log
	ReadLogEnt log_ent;
	while (readlog >> log_ent) {
		if (log_ent.last_read_memop == -1) {
				// There have no previous read access.
			prev_version[log_ent.objid] = log_ent.version;
			continue;
		}

		int prev_ver = prev_version[log_ent.objid];

		// if there's no previous "read log", prev_version[objid] defaults
		// to 0, which is correct. int prev_ver = prev_version[log_ent.objid];
		if (read_objid == log_ent.objid) {
			if (prev_ver == version) {
				result_memop = log_ent.last_read_memop;
				return true;
			} else if (prev_ver > version) {
				version_memop[read_objid].push_back(VerMemop(prev_ver,
					log_ent.last_read_memop));
				prev_version[read_objid] = log_ent.version;
				return false;
			}
			// If prev_ver < version, we can just skip it.
		} else {
			version_memop[log_ent.objid].push_back(VerMemop(prev_ver,
				log_ent.last_read_memop));
		}

		prev_version[log_ent.objid] = log_ent.version;
	}
	return false;
}
