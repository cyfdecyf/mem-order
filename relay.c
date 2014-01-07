/*
 * relay.c
 * use Numprocs threads add a counter together in a relay method
 * used to test the performance impact of spinlock waiting and backoff waiting
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <assert.h>

#include "time.h"

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

static int FinishingLine = 10000000;
static int step = 0;
static int NumProcs;
static volatile int flag;
static __thread int g_tid;
static int *ceiling;
static int *floor;

static void bind_core(long threadid) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(threadid, &set);

    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        printf("tid: %ld ", threadid);
        perror("Set affinity failed");
        exit(EXIT_FAILURE);
    }
}

static int sync_thread() {
    int v = __sync_fetch_and_add(&flag, 1);
    while (flag != NumProcs)
        asm volatile ("pause");
    return v;
}

/* The function which is called once the thread is created */
void* ThreadBody(void* _tid)
{
    g_tid = (int)(long)_tid;
    printf("thread %d created\n", g_tid);

    bind_core((long)g_tid);

    int v = sync_thread();

    // spin wait
    while (step < floor[g_tid])
        cpu_relax();

    // backoff
    /*
     *while (step < floor[g_tid]) {
     *    int i = floor[g_tid] - step;
     *    i /= 6;
     *    while (i-- >= 0)
     *        cpu_relax();
     *}
     */

    while (step < ceiling[g_tid])
        step++;

    printf("T%d done\n", g_tid);

    return NULL;
}

int main(int argc, char* argv[])
{
    pthread_t*     threads;
    pthread_attr_t attr;
    int ret, i;

    /* Parse arguments */
    if(argc != 2) {
        fprintf(stderr, "%s <numProcesors>\n", argv[0]);
        exit(1);
    }
    NumProcs = atoi(argv[1]);
    assert(NumProcs > 0 && NumProcs < 16);

    begin_clock();

    /* Initialize array of thread structures */
    threads = (pthread_t *) malloc(sizeof(pthread_t) * NumProcs);
    assert(threads != NULL);
    floor = (int *) malloc(sizeof(int) * NumProcs);
    ceiling = (int *) malloc(sizeof(int) * NumProcs);

    /* Initialize thread attribute */
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    printf("creating threads\n");
    for(i=0; i < NumProcs; i++) {
        floor[i] = FinishingLine / NumProcs * i;
        ceiling[i] = FinishingLine / NumProcs * (i + 1);
        printf("floor %d ceiling %d\n", floor[i], ceiling[i]);
        ret = pthread_create(&threads[i], &attr, ThreadBody, (void *)(long)i);
        assert(ret == 0);
    }

    /* Wait for each of the threads to terminate */
    for(i=0; i < NumProcs; i++) {
        ret = pthread_join(threads[i], NULL);
        assert(ret == 0);
    }

    end_clock();
    fflush(stdout);
    pthread_attr_destroy(&attr);

    return 0;
}

