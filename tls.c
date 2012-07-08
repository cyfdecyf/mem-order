#include "tls.h"
#include <stdio.h>
#include <stdlib.h>

pthread_key_t tid_key;

static void __constructor__ init(void) {
    if (pthread_key_create(&tid_key, NULL)) {
        printf("thr_id key creation failed\n");
        exit(1);
    }
}
