#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>

FILE *new_log(const char *name, long id);
FILE *open_log(const char *name, long id);

#endif
