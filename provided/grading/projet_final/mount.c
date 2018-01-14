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
#define ONE_BYTE (1)

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

    u->f = fopen(filename, "r+");
    if (u->f == NULL) { // Maybe there was just a PATH_TOKEN in the way...
        while (*filename == PATH_TOKEN) {
            filename += 1;
        }

        // We retry without the PATH_TOKENs.
        u->f = fopen(filename, "r+");
        if (u->f == NULL) { // Here we really have to return an error.
            return ERR_IO;
        }
    }

    uint8_t temp[SECTOR_SIZE];

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

    u->fbm = bm_alloc((uint64_t) u->s.s_block_start + 1, (uint64_t) (u->s.s_fsize - 1));
    u->ibm = bm_alloc((uint64_t) u->s.s_inode_start, (uint64_t) (u->s.s_isize * INODES_PER_SECTOR - 1));
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
    //correcteur : il faudrait tester que fbm et ibm soient pas NULL avant de les free (en plus c'est clairement un cas de figure qui peut se produire, puisque que si dans mountv6 l'alloc des bmblock foire, vous appelez umount) (-0.5)
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
                    //correcteur : pourquoi ne pas appeler également ici fill_fbm_helper ? Comme ça vous n'avez qu'à appeler qu'une fonction de fill dans le mount, et en plus vous économisez un passage sur tout les inodes (que vous faites également dans fill_fbm) (-1 dans fill_fbm)
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
 * @return 0 on success, <0 on error
 */
int mountv6_mkfs(const char *filename, uint16_t num_blocks, uint16_t num_inodes)
{
    M_REQUIRE_NON_NULL(filename);

    // Creation of the superblock.
    struct superblock s;
    memset(&s, 0, sizeof(struct superblock));
    s.s_isize = num_inodes / INODES_PER_SECTOR;
    if (num_inodes % INODES_PER_SECTOR != 0) {
        s.s_isize = (uint16_t) (s.s_isize + 1);
    }
    s.s_fsize = num_blocks;
    s.s_inode_start = SUPERBLOCK_SECTOR + 1;
    s.s_block_start = (uint16_t) (s.s_inode_start + s.s_isize);

    // Check if the sizes are correct.
    if (s.s_fsize < s.s_isize + num_inodes) {
        return ERR_NOT_ENOUGH_BLOCS;
    }

    // Creation of a new binary file that is the new file system.
    while (*filename == PATH_TOKEN) {
        //correcteur : c'était pas indispensable ça, fopen s'en occupe déjà normalement.
        filename += 1;
    }
    //correcteur : problème, vous ne fermez jamais ce file dans les cas d'erreurs de cette fonction (-1)
    FILE* newFileSystem = fopen(filename, "wb");
    if (newFileSystem == NULL) {
        return ERR_IO;
    }

    // Bootblock initialization and writing.
    unsigned char tempSector[SECTOR_SIZE];
    memset(tempSector, 0, SECTOR_SIZE);
    tempSector[BOOTBLOCK_MAGIC_NUM_OFFSET] = BOOTBLOCK_MAGIC_NUM;
    int feedback = sector_write(newFileSystem, BOOTBLOCK_SECTOR, tempSector);
    if (feedback != 0) {
        return feedback;
    }

    // Put the superblock created earlier.
    memset(tempSector, 0, SECTOR_SIZE);
    memcpy(tempSector, &s, sizeof(struct superblock));
    feedback = sector_write(newFileSystem, SUPERBLOCK_SECTOR, tempSector);
    if (feedback != 0) {
        return feedback;
    }

    // We write the inode for the ROOT directory.
    struct inode rootInode;
    memset(&rootInode, 0, sizeof(struct inode));
    rootInode.i_mode = IFDIR | IALLOC;

    struct inode tempInodes[INODES_PER_SECTOR];
    memset(tempInodes, 0, sizeof(tempInodes));
    memcpy(&tempInodes[ROOT_INUMBER], &rootInode, sizeof(struct inode));

    feedback = sector_write(newFileSystem, s.s_inode_start, tempInodes);
    if (feedback != 0) {
        return feedback;
    }

    // Fill with empty inodes.
    memset(tempInodes, 0, sizeof(tempInodes));
    for (uint32_t block = (uint32_t) (s.s_inode_start + 1); block < s.s_inode_start + s.s_isize; ++block) {
        feedback = sector_write(newFileSystem, block, tempInodes);
        if (feedback != 0) {
            return feedback;
        }
    }

    fclose(newFileSystem);
    newFileSystem = NULL;

    return 0;
}
