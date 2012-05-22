#include <stdio.h>
#include <stdlib.h>

/*#define CAREFUL*/

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        printf("Usage: infer <tid> <nobj> <logfile>\n");
        exit(1);
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
    if (!warlog) {
        printf("Can't open write log file\n");
        exit(1);
    }

    int *last_read_version = malloc(nobj * sizeof(*last_read_version));

    int read_memop, objid, version, last_read_memop;
    while (1) {
#ifdef CAREFUL
        int n = fscanf(rlog, "%d %d %d %d", &read_memop, &objid, &version, &last_read_memop);
#else
        fscanf(rlog, "%d %d %d %d", &read_memop, &objid, &version, &last_read_memop);
#endif
        if (feof(rlog))
            break;
#ifdef CAREFUL
        if (n != 4) {
            printf("Error in log, processed %d log\n", cnt);
        }
#endif

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
