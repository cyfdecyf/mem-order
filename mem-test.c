#include "mem.h"
#include <assert.h>

int main(int argc, const char *argv[])
{
    assert(obj_id(objs) == 0);
    assert(obj_id((char *)objs + 4) == 0);
    assert(obj_id((char *)objs + 4) == 0);
    assert(obj_id((char *)objs + 12) == 1);
    assert(obj_id((char *)objs + 16) == 2);
    return 0;
}
