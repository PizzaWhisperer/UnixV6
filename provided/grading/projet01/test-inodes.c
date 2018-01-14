#include <stdio.h>
#include "inode.h"

int test(struct unix_filesystem *u)
{
    return inode_scan_print(u);
}
