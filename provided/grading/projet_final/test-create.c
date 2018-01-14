#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mount.h"
#include "direntv6.h"
#include "bmblock.h"
#include "sector.h"
#include "inode.h"
#include "filev6.h"
#include "error.h"

#define EMPTY_STRING ""

int test(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);

    fprintf(stderr, "==================================================================\n");

    inode_scan_print(u);

    int err = direntv6_print_tree(u, ROOT_INUMBER, EMPTY_STRING);
    if (err < 0) {
        fprintf(stderr, "ERROR: direntv6_print_tree %d\n", err);
    }

    bm_print(u->fbm);
    bm_print(u->ibm);

    struct inode inode;
    memset(&inode, 0, sizeof(struct inode));
    inode_read(u, 2, &inode);

    struct filev6 fv6;
    memset(&fv6, 0, sizeof(struct filev6));
    fv6.u = u;
    fv6.i_number = (uint16_t) bm_find_next(u->ibm);
    fv6.offset = 0;

    err = direntv6_create(u, "/ced", IFDIR | IALLOC);
    if (err < 0) {
        fprintf(stderr, "ERROR: direntv6_create %d\n", err);
    }

    err = direntv6_create(u, "/ced/mathilde.txt", IALLOC);
    if (err < 0) {
        fprintf(stderr, "ERROR: direntv6_create %d\n", err);
    }

    fprintf(stderr, "==================================================================\n");

    inode_scan_print(u);

    err = direntv6_print_tree(u, ROOT_INUMBER, EMPTY_STRING);
    if (err < 0) {
        fprintf(stderr, "ERROR: direntv6_print_tree %d\n", err);
    }

    bm_print(u->fbm);
    bm_print(u->ibm);

    return 0;
}
