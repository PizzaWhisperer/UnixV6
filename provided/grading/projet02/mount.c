#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "mount.h"
#include "error.h"
#include "sector.h"
#include "bmblock.h"
#include "inode.h"

#define BYTE_SIZE (8)
#define NAMES_LENGTH (14)

/**
 * @brief  mount a unix v6 filesystem
 * @param filename name of the unixv6 filesystem on the underlying disk (IN)
 * @param u the filesystem (OUT)
 * @return 0 on success; <0 on error
 */
int mountv6(const char *filename, struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(u);

    memset(u, 0, sizeof(struct unix_filesystem));

    u->f = fopen(filename, "r");
    if (u->f == NULL) {
        return ERR_IO;
    }

    unsigned char temp[SECTOR_SIZE];

    int readFeedback = sector_read(u->f, BOOTBLOCK_SECTOR, temp);
    if (readFeedback != 0) {
        umountv6(u);

        return readFeedback;
    }

    if (temp[BOOTBLOCK_MAGIC_NUM_OFFSET] != BOOTBLOCK_MAGIC_NUM) {
        umountv6(u);

        return ERR_BADBOOTSECTOR;
    }

    readFeedback = sector_read(u->f, SUPERBLOCK_SECTOR, temp);
    if (readFeedback != 0) {
        umountv6(u);

        return readFeedback;
    }

    memcpy(&u->s, temp, SECTOR_SIZE);

    u->fbm = bm_alloc((uint64_t) u->s.s_block_start + 1, (uint64_t) (u->s.s_fsize));
    u->ibm = bm_alloc((uint64_t) u->s.s_inode_start, (uint64_t) (u->s.s_isize * INODES_PER_SECTOR));
    if (u->fbm == NULL || u->ibm == NULL) {
        umountv6(u);

        return ERR_BAD_PARAMETER;
    }

    fill_fbm(u);
    fill_ibm(u);

    return 0;
}

/**
 * @brief print to stdout the content of the superblock
 * @param u - the mounted filesytem
 */
void mountv6_print_superblock(const struct unix_filesystem *u)
{
    printf("**********FS SUPERBLOCK START**********\n");

    if (u != NULL) {
        printf("%-20s: %" PRIu16 "\n", "s_isize", u->s.s_isize);
        printf("%-20s: %" PRIu16 "\n", "s_fsize", u->s.s_fsize);
        printf("%-20s: %" PRIu16 "\n", "s_fbmsize", u->s.s_fbmsize);
        printf("%-20s: %" PRIu16 "\n", "s_ibmsize", u->s.s_ibmsize);
        printf("%-20s: %" PRIu16 "\n", "s_inode_start", u->s.s_inode_start);
        printf("%-20s: %" PRIu16 "\n", "s_block_start", u->s.s_block_start);
        printf("%-20s: %" PRIu16 "\n", "s_fbm_start", u->s.s_fbm_start);
        printf("%-20s: %" PRIu16 "\n", "s_ibm_start", u->s.s_ibm_start);
        printf("%-20s: %" PRIu8 "\n", "s_flock", u->s.s_flock);
        printf("%-20s: %" PRIu8 "\n", "s_ilock", u->s.s_ilock);
        printf("%-20s: %" PRIu8 "\n", "s_fmod", u->s.s_fmod);
        printf("%-20s: %" PRIu8 "\n", "s_ronly", u->s.s_ronly);
        printf("%-20s: [%" PRIu16 "] %" PRIu16 "\n", "s_time", u->s.s_time[0], u->s.s_time[1]);
    } else {
        printf("NULL ptr");
    }

    printf("**********FS SUPERBLOCK END**********\n");

    fflush(stdout);
}

/**
 * @brief umount the given filesystem
 * @param u - the mounted filesytem
 * @return 0 on success; <0 on error
 */
int umountv6(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);

    free(u->fbm);
    u->fbm = NULL;
    free(u->ibm);
    u->ibm = NULL;

    if (!ferror(u->f) || fclose(u->f) != 0) {
        return ERR_IO;
    }
    u->f = NULL;

    return 0;
}

/**
 * @brief read inodes and fill u bmblock array
 * @param u to fill
 */
void fill_ibm(struct unix_filesystem *u)
{
    if (u != NULL) {
        size_t sectStart = u->s.s_inode_start;
        size_t nbrSect = u->s.s_isize;

        struct inode tempInodes[INODES_PER_SECTOR];

        size_t sectCount = 0;
        while (sectCount < nbrSect) {
            int readFeedback = sector_read(u->f, (uint32_t) (sectStart + sectCount), tempInodes);
            for (size_t i = 0; i < INODES_PER_SECTOR; ++i) {
                if (readFeedback || (tempInodes[i].i_mode & IALLOC)) {
                    bm_set(u->ibm, (uint64_t) (i + sectCount * INODES_PER_SECTOR));
                }
            }

            sectCount += 1;
        }
    }
}

/**
 * @brief helper function to fill the fbm
 * @param inr the inode number
 * @param inode pointer to the inode struct to fill
 */
void fill_fbm_helper(struct unix_filesystem *u, struct inode* inode, const uint16_t inr)
{
    if (!inode_read(u, inr, inode)) {
        int sector = 0;
        int32_t offset = 0;

        while ((sector = inode_findsector(u, inode, offset)) > 0) {
            if (inode_getsize(inode) > SECT_DOWN_LIM) {

                // We are using indirect sectors
                int32_t offsetIAddr = offset / ADDRESSES_PER_SECTOR;
                if (0 <= offsetIAddr && offsetIAddr < ADDR_SMALL_LENGTH) {
                    bm_set(u->fbm, inode->i_addr[offsetIAddr]);
                }
            }
            bm_set(u->fbm, (uint64_t) sector);

            offset += 1;
        }
    }
}

/**
 * @brief read sectors and fill u bmblock array
 * @param u to fill
 */
void fill_fbm(struct unix_filesystem *u)
{
    if (u != NULL) {
        struct inode inode;
        memset(&inode, 0, sizeof(struct inode));

        // Root not in our ibm bitmap, we're adding sectors used by hand.
        fill_fbm_helper(u, &inode, ROOT_INUMBER);

        memset(&inode, 0, sizeof(struct inode));
        for (uint64_t j = u->ibm->min; j <= u->ibm->max; ++j) {
            fill_fbm_helper(u, &inode, (uint16_t) j);

            memset(&inode, 0, sizeof(struct inode));
        }
    }
}

/**
 * @brief create a new filesystem
 * @param num_blocks the total number of blocks (= max size of disk), in sectors
 * @param num_inodes the total number of inodes
 */
int mountv6_mkfs(const char *filename, uint16_t num_blocks, uint16_t num_inodes)
{
    M_REQUIRE_NON_NULL(filename);

    if (num_blocks < (num_inodes / INODES_PER_SECTOR + num_inodes)) {
        return ERR_NOT_ENOUGH_BLOCS;
    }

    /// MAYBE HAVE TO CALLOC???
    struct superblock s;
    memset(&s, 0, sizeof(struct superblock));
    s.s_isize = num_inodes / INODES_PER_SECTOR;
    s.s_fsize = num_blocks;
    s.s_inode_start = SUPERBLOCK_SECTOR + 1;
    s.s_block_start = (uint16_t) (SUPERBLOCK_SECTOR + 1 + num_inodes / INODES_PER_SECTOR);

    /* struct superblock s = {
         num_inodes / INODES_PER_SECTOR,
         num_blocks,
         0,
         0,
         SUPERBLOCK_SECTOR + 1,
         SUPERBLOCK_SECTOR + 1 + num_inodes / INODES_PER_SECTOR,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0
     };

     *struct superblock {

     uint16_t    s_isize;        // size in sectors of the inodes /
     uint16_t    s_fsize;        // size in sectors of entire volume *
     uint16_t    s_fbmsize;      / size in sectors of the freelist bitmap/
     uint16_t    s_ibmsize;      / size in sectors of the inode bitmap /
     uint16_t    s_inode_start;  / first sector with inodes /
     uint16_t    s_block_start;  / first sector with data *
     uint16_t    s_fbm_start;    / first sector with the freebitmap (==2)
     uint16_t    s_ibm_start;    / first sector with the inode bitmap *

     uint8_t     s_flock;        / lock during free list manipulation *
     uint8_t     s_ilock;        / lock during I list manipulation *
     uint8_t     s_fmod;         / super block modified flag *
     uint8_t     s_ronly;        / mounted read-only flag *
     uint16_t    s_time[2];      / current date of last update *
     uint16_t    pad[244];       / unused entries
                                   padding to ensure sizeof(superblock) == SECTOR_SIZE *
     };*/

    return 0;
}
