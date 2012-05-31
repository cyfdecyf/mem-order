#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

FILE *new_log(const char *name, long id);
FILE *open_log(const char *name, long id);

enum { LOG_BUFFER_SIZE = 4 * 1024 * 1024 }; // For testing, make it small to make enlarge necessary.

void *open_mapped_log(const char *name, int id, int *fd);
void *enlarge_mapped_log(void *buf, int fd);

#endif
