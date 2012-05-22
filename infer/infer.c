#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		printf("Usage: infer <tid> <nobj> <logfile>\n");
	}

	int tid = atoi(argv[1]);
	int nobj = atoi(argv[2]);

	// Open read log
	FILE *rlog = fopen(argv[3], "r");
	if (!rlog) {
		printf("Can't open read log file\n");
		exit(1);
	}

	// Create war log
	const int path_max = 255;
	char warpath[path_max];
	snprintf(warpath, path_max, "war-%d", tid);
	FILE *warlog = fopen(warpath, "w");

	int *last_read_version = malloc(nobj * sizeof(*last_read_version));

	int read_memop, objid, version, last_read_memop;
	int n;
	while (!feof(rlog)) {
		n = fscanf(rlog, "%d %d %d %d", &read_memop, &objid, &version, &last_read_memop);
		if (n != 4) {
			printf("Error in log\n");
		}

		// No previous read, no dependency needed
		if (last_read_memop == -1) {
			last_read_version[objid] = version;
			continue;
		}

		fprintf(warlog, "%d %d %d\n", tid, last_read_memop, last_read_version[objid]);
		last_read_version[objid] = version;
	}

	return 0;
}