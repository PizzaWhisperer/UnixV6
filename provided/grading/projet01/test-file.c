#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filev6.h"
#include "error.h"
#include "inode.h"
#include "sha.h"

#define INODE_3 3
#define INODE_5 5

void print_inode(struct unix_filesystem *u, struct filev6* fv6, uint16_t inodeNumber)
{
    int feedback = filev6_open(u, inodeNumber, fv6);
    if (feedback != 0) {
        fprintf(stderr, "\nfilev6_open failed for inode #%u.\n", inodeNumber);
    } else {
        printf("\nPrinting inode #%u:\n", inodeNumber);

        inode_print(&fv6->i_node);
        if (fv6->i_node.i_mode & IFDIR) {
            printf("which is a directory.\n");
        } else {
            unsigned char buffer[SECTOR_SIZE + 1];

            feedback = filev6_readblock(fv6, buffer);
            if (feedback < 0) {
                fprintf(stderr, "filev6_readblock failed for inode #%u.\n", inodeNumber);
            } else {
                buffer[SECTOR_SIZE] = '\0';
                printf("the first sector of data of which contains:\n");
                printf("%s", buffer);
            }
        }
    }

    fflush(stdout);
}

int test(struct unix_filesystem *u)
{
    struct filev6 fv6;

    ///***Inode3******/
    print_inode(u, &fv6, INODE_3);

    ///***Inode5******/
    print_inode(u, &fv6, INODE_5);

    printf("\n----\n\nListing inodes SHA:\n");
    for (uint16_t i = 0; i < (u->s.s_isize * SECTOR_SIZE / (uint16_t) sizeof(struct inode)); ++i) {
        memset(&fv6.i_node, 0, sizeof(struct inode));
        inode_read(u, i, &fv6.i_node);
        print_sha_inode(u, fv6.i_node, (int) i);
    }

    return 0;
}
