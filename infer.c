#include "mem.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("Usage: infer <tid> <logfile>\n");
        exit(1);
    }

    int tid = atoi(argv[1]);

    // Open read log
    FILE *rlog = fopen(argv[2], "r");
    if (!rlog) {
        printf("Can't open read log file\n");
        exit(1);
    }

    // Create war log
    const int path_max = 255;
    char warpath[path_max];
    snprintf(warpath, path_max, "war-%d", tid);
    FILE *warlog = fopen(warpath, "w");
    if (!warlog) {
        printf("Can't open write log file\n");
        exit(1);
    }

    int *last_read_version = malloc(NOBJS * sizeof(*last_read_version));

    int read_memop, objid, version, last_read_memop;
    while (fscanf(rlog, "%d %d %d %d\n", &objid, &version, &read_memop,
        &last_read_memop) == 4) {

        if (last_read_memop == -1) {
            // No previous read, no dependency needed
            last_read_version[objid] = version;
            continue;
        }

        fprintf(warlog, "%d %d %d %d\n", objid, last_read_version[objid], last_read_memop, tid);
        last_read_version[objid] = version;
    }
    if (! feof(rlog)) {
        printf("Error in log\n");
    }

    fclose(rlog);
    fclose(warlog);

    return 0;
}
