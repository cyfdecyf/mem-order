#include "mem-record.h"
#include "tsx/rtm.h"
#include <stdio.h>

// Provide atomic version & memory access by RTM (seqlock as fallback handler).

uint32_t mem_read(tid_t tid, uint32_t *addr) {
    version_t version;
    uint32_t val;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    int ret = 0;
    if ((ret = _xbegin()) == _XBEGIN_STARTED) {
        version = info->version;
        val = *addr;
        _xend();
    } else {
        fprintf(stderr, "T%d R%ld fallback reason %d\n", g_tid, memop, ret);
        do {
            version = info->version;
            while (unlikely(version & 1)) {
                cpu_relax();
                version = info->version;
            }
            barrier();
            val = *addr;
            barrier();
        } while (version != info->version);
    }

    struct last_objinfo *lastobj = &g_last[objid];

    if (lastobj->version != version) {
        log_order(tid, objid, version, lastobj);
        // Update version so that following write after read don't need logs.
        lastobj->version = version;
    }

#ifdef DEBUG_ACCESS
    log_access('R', objid, version, val);
#endif

    lastobj->memop = memop;
    memop++;
    return val;
}

void mem_write(tid_t tid, uint32_t *addr, uint32_t val) {
    version_t version;
    objid_t objid = calc_objid(addr);
    struct objinfo *info = &g_objinfo[objid];

    int ret = 0;
    if ((ret = _xbegin()) == _XBEGIN_STARTED) {
        version = info->version;
        barrier();
        *addr = val;
        barrier();
        info->version += 2;
        _xend();
        __sync_synchronize();
    } else {
        fprintf(stderr, "T%d W%ld fallback reason %d\n", g_tid, memop, ret);
        spin_lock(&info->write_lock);

        version = info->version;
        barrier();

        // Odd version means that there's writer trying to update value.
        info->version++;
        barrier();
        *addr = val;
        // This barrier disallows read to happen before the write.
        // The explicit barrier here may also make the compiler unnecessary here.
        __sync_synchronize();
        info->version++;

        spin_unlock(&info->write_lock);
    }

    struct last_objinfo *lastobj = &g_last[objid];

    if (lastobj->version != version) {
        log_order(tid, objid, version, lastobj);
    }

#ifdef DEBUG_ACCESS
    log_access('W', objid, version, val);
#endif

    lastobj->memop = ~memop; // flip to mark last memop as write
    lastobj->version = version + 2;
    memop++;
}

