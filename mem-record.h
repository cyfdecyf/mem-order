#ifndef _MEM_RECORD_H
#define _MEM_RECORD_H

#include "mem.h"
#include "spinlock.h"

// Version information for each shared object.
struct objinfo {
    volatile version_t version;
    spinlock write_lock;
};

// Information of each thread's last access.
struct last_objinfo {
    version_t version;
    memop_t memop;
};

// Object version shared by all threads.
extern struct objinfo *g_objinfo;

extern __thread struct last_objinfo *g_last;

// Memory operation count for each thread.
extern __thread memop_t memop;

// Order recording.
void log_order(objid_t objid, version_t current_version, struct last_objinfo *lastobj);

#ifdef DEBUG_ACCESS
void log_access(char acc, objid_t objid, version_t ver, uint32_t val);
#endif

#endif
