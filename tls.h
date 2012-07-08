#ifndef TLS_H
#define TLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

// Macro to emulate thread local storage

// Store TLS data in a global array, use thread id to get thread local
// data.

extern pthread_key_t tid_key;
#define TLS_tid() \
    tid_t tid = (tid_t)(long)pthread_getspecific(tid_key)

// Defines the global array.
#define DEFINE_TLS_GLOBAL(type, var) \
    type *var##_tls
#define ALLOC_TLS_GLOBAL(nthr, var) \
    var##_tls = calloc_check(nthr, sizeof(*(var##_tls)), #var)

#define TLS_GLOBAL(var) (var##_tls)

// Use thread id to get thread local data.
// This macro must be used after the TLS_THRID macro.
#define TLS(var) (var##_tls[tid])

#ifdef __cplusplus
}
#endif

#endif