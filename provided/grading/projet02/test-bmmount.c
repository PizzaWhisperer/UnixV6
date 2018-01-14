#include <stdio.h>
#include "mount.h"
#include "bmblock.h"
#include "error.h"

int test(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);

    bm_print(u->fbm);
    bm_print(u->ibm);

    return 0;
}
