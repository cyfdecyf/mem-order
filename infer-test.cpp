#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE Infer

#include "infer.hpp"
#include <sstream>
#include <boost/test/unit_test.hpp>
using namespace std;

struct ReadAtVerTestCase {
	int version;
	int objid;
	int result_memop;
	bool found;
};

ostream& operator<<(ostream &os, ReadAtVerTestCase &rhs) {
	os << "@: " << rhs.version
	   << " objid: " << rhs.objid
	   << " result_memop: " << rhs.result_memop
	   << " found: " << rhs.found;
	return os;
}

BOOST_AUTO_TEST_CASE(read_at_version_on_obj) {
	/*
		Test log contains log for the following read on two objects.
		Starting L in the read means this is in log.

		objid     0          1         3
		        L 0 @1
		                     1 @0
		                     2 @0
		         3 @1
		         		   L 4 @5
		         		   L 5 @6
		         		     6 @6
		         7 @1
		         8 @1
		                     9 @6
		       L 10 @7
		                             L 11 @3
		                               12 @3
		                   L 14 @10
		                   			 L 15 @5
	*/
	ReadLog *readlog_p = new ReadLog(3, "testdata/rec-rd-0");
	ReadLog &readlog = *readlog_p;

	int result_memop = -1;
	bool found = false;

	ReadAtVerTestCase test_data[] = {
		// obj 0
		{0, 1, 2, true},
		{0, 0, -1, false},
		{1, 0, 8, true},
		{1, 1, -1, false},
		{2, 0, -1, false},
		{3, 0, -1, false},
		{2, 1, -1, false},
		{3, 1, -1, false},
		{4, 1, -1, false},
		{2, 2, -1, false},
		{3, 2, 12, true},
		{5, 1, 4, true},
		{6, 0, -1, false},
		{6, 1, 9, true},
		{7, 0, -1, false}, // The last read info is not dumped, so can't find this
		{4, 2, -1, false},
		{9, 1, -1, false},
		{10, 1, -1, false}, // The last read info is not dumped, so can't find this
		{5, 2, -1, false}, // The last read info is not dumped, so can't find this
	};

	for (unsigned int i = 0; i < sizeof(test_data)/sizeof(*test_data); i++) {
		found = readlog.read_at_version_on_obj(test_data[i].version,
			test_data[i].objid, result_memop);
		ostringstream found_os;
		found_os << "found = " << found << " test_data[" << i << "] = "
		   << test_data[i];
		BOOST_CHECK_MESSAGE(found == test_data[i].found, found_os.str());

		ostringstream os;
		os << "actual memop: " << result_memop
		   << " test_data[" << i << "] = " << test_data[i];
		BOOST_REQUIRE_MESSAGE(result_memop == test_data[i].result_memop, os.str());
		result_memop = -1;
	}

	delete readlog_p;
}
