#include "mem.h"
#include <assert.h>

int main(int argc, const char *argv[])
{
    assert(obj_id(objs) == 0);
    assert(obj_id(objs + 4) == 0);
    assert(obj_id(objs + 4) == 0);
    assert(obj_id(objs + 12) == 1);
    assert(obj_id(objs + 16) == 2);
    return 0;
}
