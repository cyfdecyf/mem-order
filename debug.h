#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG

#include <stdio.h>
#include <assert.h>

# define DPRINTF_LINE(fmt, ...) \
    fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

# define DPRINTF(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

# define ASSERT_PRINTF(exp, fmt, ...) \
    do { \
      if (!(exp)) \
          fprintf(stderr, "%s:%d: %s: " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
      assert((exp)); \
    } while (0)

#else

# define DPRINTF(fmt, ...)
# define ASSERT_PRINTF(exp, fmt, ...)

#endif

#endif /* _DEBUG_H */
