/*
 * RACEY: a program print a result which is very sensitive to the
 * ordering between processors (races).
 *
 * It is important to "align" the short parallel executions in the
 * simulated environment. First, a simple barrier is used to make sure
 * thread on different processors are starting at roughly the same time.
 * Second, each thread is bound to a physical cpu. Third, before the main
 * loop starts, each thread use a tight loop to gain the long time slice
 * from the OS scheduler.
 *
 * NOTE: This program need to be customized for your own OS/Simulator 
 * environment. See TODO places in the code.
 *
 * Author: Min Xu <mxu@cae.wisc.edu>
 * Main idea: Due to Mark Hill
 * Created: 09/20/02
 *
 * Modified on 13/12/17 by Chen Yufei to test my record/replay algorithm.
 *
 * Compile (on Solaris for Simics) :
 *   cc -mt -o racey racey.c magic.o
 * (on linux with gcc)
 *   gcc -o racey racey.c -lpthread
 */

#include "mem.h"

#include <mcheck.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <assert.h>

#undef DEBUG
/*#define DEBUG*/

#ifdef DEBUG
# define DPRINTF(fmt, ...) \
    printf(fmt, ##__VA_ARGS__)
#else
# define DPRINTF(fmt, ...)
#endif

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

// TODO: replace assert(0) with your own function that marks a program phase.  
// Example being a simic "magic" instruction, or VMware backdoor call. Printf 
// should be avoided since it may cause app/OS interaction, even de-scheduling.
//#define PHASE_MARKER assert(0)

// Marker in COREMU.
#ifdef COREMU

#define PHASE_MARKER \
    do { \
        asm volatile("int $0x77"); \
    } while(0)

#else

#define PHASE_MARKER

#endif

// Use non round number to check corner cases for batch log taking.
#define   MAX_LOOP 2067
#define   MAX_ELEM 64

#define   PRIME1   103072243
#define   PRIME2   103995407

/* simics checkpoints Proc Ids (not used, for reference only) */
int               Ids_01p[1]  = { 6};
int               Ids_02p[2]  = { 6,  7};
int               Ids_04p[4]  = { 0,  1,  4,  5};
int               Ids_08p[8]  = { 0,  1,  4,  5,  6,  7,  8,  9};
int               Ids_16p[16] = { 0,  1,  4,  5,  6,  7,  8,  9,
                                 10, 11, 12, 13, 14, 15, 16, 17};

int               NumProcs;

volatile int flag;
static int sync_thread() {
    int v = __sync_fetch_and_add(&flag, 1);
    while (flag != NumProcs)
        asm volatile ("pause");
    return v;
}

/* shared variables */
uint32_t g_sig[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
union {
  /* 64 bytes cache line */
  char    b[64];
  uint32_t value;
} g_m[MAX_ELEM];

objid_t calc_objid_racey(void *addr) {
    objid_t id = ((long)addr - (long)g_m)/sizeof(g_m[0]);
    assert(id < MAX_ELEM);
    /*printf("T%d objid %d\n", g_tid, id);*/
    return id;
}

/* the mix function */
uint32_t mix(uint32_t i, uint32_t j) {
  /*printf("T%d mix %d %d\n", g_tid, i, j);*/
  return (i + j * PRIME2) % PRIME1;
}

/* The function which is called once the thread is created */
void* ThreadBody(void* _tid)
{
  g_tid = (int)(long)_tid;
  int i;
  DPRINTF("thread %d created\n", g_tid);

  mem_init_thr(g_tid);

  /*
   * Thread Initialization:
   *
   * Bind the thread to a processor.  This will make sure that each of
   * threads are on a different processor.  ProcessorIds[g_tid]
   * specifies the processor ID which the thread is binding to.
   */
  // TODO:
  // Bind this thread to ProcessorIds[g_tid]
  // use processor_bind(), for example on solaris.
  bind_core((long)g_tid);

  /*DPRINTF("T%d seizing cpu &i = %p\n", g_tid, &i);*/
  /* seize the cpu, roughly 0.5-1 second on ironsides */
  // for(i=0; i<0x07ffffff; i++) {};
  /*DPRINTF("T%d cpu seized\n", g_tid);*/

  /* simple barrier, pass only once */
  int v = sync_thread();
  if (v == NumProcs) PHASE_MARKER; // start of parallel phase

  /*
   * main loop:
   *
   * Repeatedly using function "mix" to obtain two array indices, read two 
   * array elements, mix and store into the 2nd
   *
   * If mix() is good, any race (except read-read, which can tell by software)
   * should change the final value of mix
   */
  for(i = 0 ; i < MAX_LOOP; i++) {
    uint32_t num = g_sig[g_tid];
    uint32_t index1 = num%MAX_ELEM;
    uint32_t index2;
    /*num = mix(num, g_m[index1].value);*/
    num = mix(num, mem_read(g_tid, (uint32_t *)&g_m[index1].value));
    index2 = num%MAX_ELEM;
    /*num = mix(num, g_m[index2].value);*/
    num = mix(num, mem_read(g_tid, (uint32_t *)&g_m[index2].value));
    /*g_m[index2].value = num;*/
    mem_write(g_tid, (uint32_t *)&g_m[index2].value, num);
    g_sig[g_tid] = num;
    /* Optionally, yield to other processors (Solaris use sched_yield()) */
    /*pthread_yield();*/
  }
  mem_finish_thr();
  DPRINTF("T%d done\n", g_tid);

  return NULL;
}

int
main(int argc, char* argv[])
{
  pthread_t*     threads;
  pthread_attr_t attr;
  int            ret, i;
  uint32_t       mix_sig;

  /*mtrace();*/

  /*
   *printf("sig: %p, size: %lu\n", g_sig, sizeof(g_sig));
   *printf("m: %p, size: %lu\n", g_m, sizeof(g_m));
   *printf("ThreadBody: %p\n", ThreadBody);
   *printf("mix: %p\n", mix);
   */

  /* Parse arguments */
  if(argc != 2) {
    /*fprintf(stderr, "%s <numProcesors> <pIds>\n", argv[0]);*/
    fprintf(stderr, "%s <numProcesors>\n", argv[0]);
    exit(1);
  }
  NumProcs = atoi(argv[1]);
  assert(NumProcs > 0 && NumProcs < 16);

  /* Init for record&replay */
  calc_objid = calc_objid_racey;
  mem_init(NumProcs, MAX_ELEM);

  /*
   *assert(argc == (NumProcs+2));
   *ProcessorIds = (int *) malloc(sizeof(int) * NumProcs);
   *assert(ProcessorIds != NULL);
   *for(i=0; i < NumProcs; i++) {
   *  ProcessorIds[i] = atoi(argv[i+2]);
   *}
   */

  DPRINTF("initialize mix array\n");
  /* Initialize the mix array */
  for(i = 0; i < MAX_ELEM; i++) {
    g_m[i].value = mix(i,i);
  }

  /* Initialize array of thread structures */
  threads = (pthread_t *) malloc(sizeof(pthread_t) * NumProcs);
  assert(threads != NULL);

  /* Initialize thread attribute */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  DPRINTF("creating threads\n");
  for(i=0; i < NumProcs; i++) {
    /* ************************************************************
     * pthread_create takes 4 parameters
     *  p1: threads(output)
     *  p2: thread attribute
     *  p3: start routine, where new thread begins
     *  p4: arguments to the thread
     * ************************************************************ */
    ret = pthread_create(&threads[i], &attr, ThreadBody, (void *)(long)i);
    assert(ret == 0);
  }

  /* Wait for each of the threads to terminate */
  for(i=0; i < NumProcs; i++) {
    ret = pthread_join(threads[i], NULL);
    assert(ret == 0);
  }

  /* compute the result */
  mix_sig = g_sig[0];
  for(i = 1; i < NumProcs ; i++) {
    mix_sig = mix(g_sig[i], mix_sig);
  }

  PHASE_MARKER; /* end of parallel phase */
  DPRINTF("end of parallel phase\n\n");

  /* print results */
  printf("Short signature: %08x\n", mix_sig);
  fflush(stdout);
  /*usleep(5);*/

  /*PHASE_MARKER;*/

  pthread_attr_destroy(&attr);

  return 0;
}

