#include "infer.hpp"
#include <iostream>
#include <sstream>
using namespace std;

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

	return 0;
}
