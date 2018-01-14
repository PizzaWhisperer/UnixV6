#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "inode.h"
#include "unixv6fs.h"
#include "sector.h"
#include "error.h"

#define SIZE0_SHIFT (16)
#define SIZE0_SHADOW (0x00FF0000)
#define SIZE1_SHADOW (0x0000FFFF)

/**
 * @brief read all inodes from disk and print out their content to
 *        stdout according to the assignment
 * @param u the filesystem
 * @return 0 on success; < 0 on error.
 */
int inode_scan_print(const struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u->f);

    size_t sectStart = u->s.s_inode_start;
    size_t nbrSect = u->s.s_isize;

    struct inode tempInodes[INODES_PER_SECTOR];

    size_t sectCount = 0;
    while (sectCount < nbrSect) {

        int readFeedback = sector_read(u->f, (uint32_t) (sectStart + sectCount), tempInodes);
        if (readFeedback != 0) {
            return readFeedback;
        }

        for (size_t i = 0; i < INODES_PER_SECTOR; ++i) {
            if (tempInodes[i].i_mode & IALLOC) {
                const char * inode_name = tempInodes[i].i_mode & IFDIR ? SHORT_DIR_NAME : SHORT_FIL_NAME;

                printf("inode %3lu (%s) len   %d\n", i + INODES_PER_SECTOR * sectCount,
                       inode_name, inode_getsize(&tempInodes[i]));
            }
        }

        sectCount += 1;
    }

    fflush(stdout);

    return 0;
}

/**
 * @brief prints the content of an inode structure
 * @param inode the inode structure to be displayed
 */
void inode_print(const struct inode *inode)
{
    printf("**********FS INODE START**********\n");

    if (inode != NULL) {
        printf("%-20s: %" PRIu16 "\n", "i_mode", inode->i_mode);
        printf("%-20s: %" PRIu8 "\n", "i_nlink", inode->i_nlink);
        printf("%-20s: %" PRIu8 "\n", "i_uid", inode->i_uid);
        printf("%-20s: %" PRIu8 "\n", "i_gid", inode->i_gid);
        printf("%-20s: %" PRIu8 "\n", "i_size0", inode->i_size0);
        printf("%-20s: %" PRIu16 "\n", "i_size1", inode->i_size1);
        printf("%-20s: %" PRIu32 "\n", "size", inode_getsize(inode));
    } else {
        printf("NULL ptr");
    }

    printf("**********FS INODE END**********\n");

    fflush(stdout);
}

/**
 * @brief read the content of an inode from disk
 * @param u the filesystem (IN)
 * @param inr the inode number of the inode to read (IN)
 * @param inode the inode structure, read from disk (OUT)
 * @return 0 on success; <0 on error
 */
int inode_read(const struct unix_filesystem *u, uint16_t inr, struct inode *inode)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(inode);

    if (inr < 1) {
        return ERR_BAD_PARAMETER;
    }
    //correcteur : désolé mon commentaire de correction dans projet01 n'avait pas été très clair, mais l'inode d'index 0 ne devrait pas pouvoir être accédée (elle peut l'être dans votre code en l'état, vous devriez renvoyer cette erreur ci dessous si inr < 1) (-1)
    if (inr > INODES_PER_SECTOR * u->s.s_isize) {
        return ERR_INODE_OUTOF_RANGE;
    }

    size_t sector = u->s.s_inode_start + (inr / INODES_PER_SECTOR);
    size_t goodInodeNbr = inr % INODES_PER_SECTOR;

    struct inode tempInodes[INODES_PER_SECTOR];

    int readFeedback = sector_read(u->f, (uint32_t) sector, tempInodes);
    if (readFeedback != 0) {
        return readFeedback;
    }

    if (!(tempInodes[goodInodeNbr].i_mode & IALLOC)) {
        return ERR_UNALLOCATED_INODE;
    } else {
        memcpy(inode, &tempInodes[goodInodeNbr], sizeof(struct inode));
    }

    return 0;
}

/**
 * @brief identify the sector that corresponds to a given portion of a file
 * @param u the filesystem (IN)
 * @param inode the inode (IN)
 * @param file_sec_off the offset within the file (in sector-size units)
 * @return >0: the sector on disk;  <0 error
 */
int inode_findsector(const struct unix_filesystem *u, const struct inode *i, int32_t file_sec_off)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(i);

    if (i->i_mode & IALLOC) {
        int32_t inodeSize = inode_getsize(i);

        if (inodeSize > SECT_UP_LIM) {
            // ERR FIL TOO LARGE
            return ERR_FILE_TOO_LARGE;
        } else {
            if (inodeSize > SECT_DOWN_LIM) {
                // FIL MID-SIZE
                int32_t offsetIAddr = file_sec_off / ADDRESSES_PER_SECTOR;
                int32_t indirectOffset = file_sec_off % ADDRESSES_PER_SECTOR;

                if (offsetIAddr >= 0 && offsetIAddr < ADDR_SMALL_LENGTH) {
                    uint16_t temp[ADDRESSES_PER_SECTOR];

                    int readFeedback = sector_read(u->f, i->i_addr[offsetIAddr], temp);
                    if (readFeedback != 0) {
                        return readFeedback;
                    }

                    return temp[indirectOffset];
                } else {
                    return ERR_OFFSET_OUT_OF_RANGE;
                }
            } else {
                // FIL SMALL-SIZE
                if (file_sec_off >= 0 && file_sec_off < ADDR_SMALL_LENGTH) {
                    return i->i_addr[file_sec_off];
                }

                return ERR_OFFSET_OUT_OF_RANGE;
            }
        }
    }

    return ERR_UNALLOCATED_INODE;
}

/**
 * @brief alloc a new inode (returns its inr if possible)
 * @param u the filesystem (IN)
 * @return the inode number of the new inode or error code on error
 */
int inode_alloc(struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u);

    int feedback = bm_find_next(u->ibm);
    if (feedback == ERR_BITMAP_FULL) {
        return ERR_NOMEM;
    } else if (feedback < 0) {
        return feedback;
    }

    bm_set(u->ibm, (uint64_t) feedback);

    return feedback;
}

/**
 * @brief write the content of an inode to disk
 * @param u the filesystem (IN)
 * @param inr the inode number of the inode to write (IN)
 * @param inode the inode structure, to write to disk (IN)
 * @return 0 on success; <0 on error
 */
int inode_write(struct unix_filesystem *u, uint16_t inr, const struct inode *inode)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(inode);

    uint32_t sect = u->s.s_inode_start + inr / (uint32_t) INODES_PER_SECTOR;

    //correcteur : vous devriez également contrôler qu'on ne puisse pas écrire l'inode d'index 0 (-1)
    if (sect > (uint32_t) (u->s.s_inode_start + u->s.s_isize - 1)) {
        return ERR_BAD_PARAMETER;
    }

    struct inode temp[INODES_PER_SECTOR];
    memset(temp, 0, sizeof(temp));

    int err = sector_read(u->f, sect , temp);
    if (err < 0) {
        return err;
    }

    memcpy(&temp[inr % INODES_PER_SECTOR], inode, sizeof(struct inode));

    int feedBack = sector_write(u->f, sect, temp);
    if(feedBack == 0) {
        bm_set(u->fbm, sect);
        bm_set(u->ibm, inr);
    }

    return feedBack;
}

/**
 * @brief set the size of a given inode to the given size
 * @param inode the inode
 * @param new_size the new size
 * @return 0 on success; <0 on error
 */
int inode_setsize(struct inode *inode, int new_size)
{
    M_REQUIRE_NON_NULL(inode);

    if (new_size < 0) {
        return ERR_NOMEM;
    }

    inode->i_size0 = (uint8_t) ((SIZE0_SHADOW & new_size) >> SIZE0_SHIFT);
    inode->i_size1 = (uint16_t) (SIZE1_SHADOW & new_size);

    return 0;
}
