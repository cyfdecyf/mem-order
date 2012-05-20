#include "infer.hpp"
#include <iostream>
#include <sstream>
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
	   << " @" << rhs.version
	   << " last_read_memop: " << rhs.last_read_memop;
	return os;
}

ReadLog::ReadLog(int nobj, const char *logpath) :
		prev_version(nobj), version_memop(nobj), prev_query_version(nobj, -1) {
	// Open read log
	readlog.open(logpath);
	if (! readlog) {
		cerr << "Read log " << logpath << " open failed" << endl;
		exit(1);
	}
}

bool ReadLog::read_at_version_on_obj(int version, int read_objid, int &result_memop) {
	// Because read and write can only get version that's larger then
	// previous one, the version parameter given to this function
	// should always increase
	assert(version > prev_query_version[read_objid]);
	prev_query_version[read_objid] = version;
	cout << "search read @" << version
		 << " on object " << read_objid << endl;

	// Search in already read version and memop info
	deque<VerMemop> &vermemop = version_memop[read_objid];
	for (deque<VerMemop>::iterator it = vermemop.begin();
		it != vermemop.end();
		++it) {
		if (it->version > version) {
			cout << "\talready read version " << it->version
				 << " so no read found." << endl;
			return false;
		}
		cout << "\tremove @" << it->version << " memop: " << it->memop << endl;
		vermemop.pop_front();
		if (it->version == version) {
			cout << "\tfound memop " << it->memop << " in already read deque" << endl;
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
			cout << "\tno previous read, log_ent: " <<  log_ent << endl;
			continue;
		}

		int prev_ver = prev_version[log_ent.objid];
		prev_version[log_ent.objid] = log_ent.version;

		// if there's no previous "read log", prev_version[objid] defaults
		// to 0, which is correct. int prev_ver = prev_version[log_ent.objid];
		if (read_objid == log_ent.objid) {
			if (prev_ver == version) {
				result_memop = log_ent.last_read_memop;
				cout << "\tFound read by log_ent: " << log_ent << endl;
				return true;
			} else if (prev_ver > version) {
				version_memop[read_objid].push_back(VerMemop(prev_ver,
					log_ent.last_read_memop));
				cout << "\tprev version " << prev_ver << " larger, adding log_ent: " << log_ent
					 << " to deque" << endl;
				return false;
			}
			// If prev_ver < version, we can just skip it.
		} else {
			cout << "\tadding to deque objid: " << log_ent.objid
			     << " log_ent version: " << prev_ver
			     << " read_memop: " << log_ent.last_read_memop
			     << " processing log_ent: " << log_ent << endl;
			version_memop[log_ent.objid].push_back(VerMemop(prev_ver,
				log_ent.last_read_memop));
		}
	}
	return false;
}
