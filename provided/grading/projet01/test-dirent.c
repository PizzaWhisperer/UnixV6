#include "mount.h"
#include "direntv6.h"

int test(struct unix_filesystem *u)
{
    return direntv6_print_tree(u, ROOT_INUMBER, "");
}
