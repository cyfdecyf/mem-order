#include "time.h"
#include <stdint.h>
#include <time.h>

uint64_t begin_tsc;
uint64_t end_tsc;
clock_t begin_time;
clock_t end_time;

static inline uint64_t get_tsc() {
    uint64_t h, l;
    asm volatile ("rdtscp" : "=d" (h), "=a" (l));
    return (h << 32) | l;
}

void begin_clock() {
    begin_tsc = get_tsc();
}

void end_clock() {
    end_tsc = get_tsc();
    printf("tsc %lf\n", (end_tsc - begin_tsc)/FREQ);
}
