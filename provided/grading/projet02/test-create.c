#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mount.h"
#include "bmblock.h"
#include "sector.h"
#include "inode.h"
#include "filev6.h"
#include "error.h"

int test(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);

    /*printf("====================TEST SECTOR WRITE===========\n");
    bm_print(u->fbm);
    char data[SECTOR_SIZE] = "Cedric is a poop";
    sector_write(u->f, bm_find_next(u->fbm), &data);
    printf("=================================\n");
    bm_print(u->fbm);

    printf("====================TEST INODE WRITE===========\n");
    bm_print(u->ibm);
    struct inode inode = {IFDIR, 0, 0, 0, 0,0};
    inode_write(u, bm_find_next(u->ibm), &inode);
    printf("=================================\n");
    bm_print(u->ibm);

    printf("====================TEST FILE CREATE===========\n");
    bm_print(u->ibm);
    struct filev6 fv6 = {u, bm_find_next(u->ibm), inode, 0};
    filev6_create(u, IFDIR, &fv6);
    printf("=================================\n");
    bm_print(u->ibm);*/

    struct inode tempInode;
    memset(&tempInode, 0, sizeof(struct inode));
    tempInode.i_mode = (uint16_t) IFDIR;

    struct filev6 fv6;
    memset(&fv6, 0, sizeof(struct filev6));
    fv6.u = u;
    fv6.i_number = (uint16_t) bm_find_next(u->ibm);
    fv6.i_node = tempInode;
    fv6.offset = 0;

    filev6_create(u, IFDIR, &fv6);

    return 0;
}
