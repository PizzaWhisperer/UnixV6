#include <stdio.h>
#include "mount.h"
#include "direntv6.h"
#include "error.h"

#define EMPTY_STRING ""

int test(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);

    return direntv6_print_tree(u, ROOT_INUMBER, EMPTY_STRING);
}
