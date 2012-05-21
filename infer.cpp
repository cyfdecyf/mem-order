#include "infer.hpp"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cassert>
using namespace std;

const string RDLOG_PATTERN = "rec-rd-";
const string WRLOG_PATTERN = "rec-wr-";
const string WARLOG_PATTERN = "war-rd-";

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

istream& operator>>(istream &is, WriteLogEnt &rhs) {
	is >> rhs.objid >>  rhs.version;
	return is;
}

ostream& operator<<(ostream &os, WriteLogEnt &rhs) {
	os << " objid: " << rhs.objid
	   << " @" << rhs.version;
	return os;
}

ReadLog::ReadLog(int nobj, const char *logpath) :
		last_read_version(nobj), version_memop(nobj), prev_query_version(nobj, -1) {
	if (logpath)
		openlog(logpath);	
}

void ReadLog::openlog(const char *logpath) {
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
	// cout << "search read @" << version
	// 	 << " on object " << read_objid << endl;

	// Search in already read version and memop info
	deque<VersionMemop> &vermemop = version_memop[read_objid];
	for (deque<VersionMemop>::iterator it = vermemop.begin();
		it != vermemop.end();
		++it) {
		if (it->version > version) {
			// cout << "\talready read version " << it->version
			// 	 << " so no read found." << endl;
			return false;
		}
		// cout << "\tremove @" << it->version << " memop: " << it->memop << endl;
		vermemop.pop_front();
		if (it->version == version) {
			// cout << "\tfound memop " << it->memop << " in already read deque" << endl;
			result_memop = it->memop;
			return true;
		}
	}

	// Search in the read log
	ReadLogEnt log_ent;
	while (readlog >> log_ent) {
		if (log_ent.last_read_memop == -1) {
			// There have no previous read access.
			last_read_version[log_ent.objid] = log_ent.version;
			// cout << "\tno previous read, log_ent: " <<  log_ent << endl;
			continue;
		}

		int last_ver = last_read_version[log_ent.objid];
		last_read_version[log_ent.objid] = log_ent.version;

		// if there's no previous "read log", last_read_version[objid] defaults
		// to 0, which is correct.
		if (read_objid == log_ent.objid) {
			if (last_ver == version) {
				result_memop = log_ent.last_read_memop;
				// cout << "\tFound read by log_ent: " << log_ent << endl;
				return true;
			} else if (last_ver > version) {
				version_memop[read_objid].push_back(VersionMemop(last_ver,
					log_ent.last_read_memop));
				// cout << "\tprev version " << last_ver << " larger, adding log_ent: " << log_ent
				// 	 << " to deque" << endl;
				return false;
			}
			// If prev_ver < version, we can just skip it.
		} else {
			// cout << "\tadding to deque objid: " << log_ent.objid
			//      << " log_ent version: " << last_ver
			//      << " read_memop: " << log_ent.last_read_memop
			//      << " processing log_ent: " << log_ent << endl;
			version_memop[log_ent.objid].push_back(VersionMemop(last_ver,
				log_ent.last_read_memop));
		}
	}
	return false;
}

WriteLog::WriteLog(int nobj, const char *logpath) : last_write_version(nobj) {
	if (logpath)
		openlog(logpath);
}

void WriteLog::openlog(const char *logpath) {
	writelog.open(logpath);
	if (! writelog) {
		cerr << "Write log " << logpath << " open failed" << endl;
		exit(1);
	}
}

bool WriteLog::next_write_version(int objid, int &version) {
	// cout << "search next_write_version objid: " << objid << endl;
	// Search in the deque	
	deque<int> &last_write = last_write_version[objid];

	if (! last_write.empty()) {
		version = last_write[0];
		last_write.pop_front();
		// cout << "\tfound in deque @" << version << endl;
		return true;
	}

	WriteLogEnt log_ent;
	while (writelog >> log_ent) {
		if (log_ent.objid == objid) {
			version = log_ent.version;
			// cout << "\tfound in log @" << version << endl;
			return true;
		}
		// Add to the corresponding last write deque
		// cout << "\tadding to deque write obj: " << log_ent.objid
		// 	 << " @" << log_ent.version << endl;
		last_write_version[log_ent.objid].push_back(log_ent.version);
	}
	// cout << "\tnot more write log" << endl;
	return false;
}

Infer::Infer(int tid, int nthr, int nobj, const char *logdir) :
		wlog(nobj), rlog(nobj) {
	ostringstream os;

	for (int i = 0; i < nthr; i++) {
		os << logdir << "/" << RDLOG_PATTERN << i;
		rlog[i] = new ReadLog(nobj, os.str().c_str());
		os.str("");

		os << logdir << "/" << WRLOG_PATTERN << i;
		wlog[i] = new WriteLog(nobj, os.str().c_str());
		os.str("");
	}

	os << logdir << "/" << WARLOG_PATTERN << tid;
	war_out.open(os.str().c_str());
	if (! war_out) {
		cerr << "Cant't open write-after-read log" << os.str() << endl;
	}
	os.str("");
}

int Infer::following_write_version(int start_version) {
	return start_version;
}

void Infer::infer() {
}
