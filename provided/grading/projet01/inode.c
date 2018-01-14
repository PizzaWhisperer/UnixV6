#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "inode.h"
#include "unixv6fs.h"
#include "sector.h"
#include "error.h"

#define SECT_DOWN_LIM (8 * SECTOR_SIZE)
#define SECT_UP_LIM (7 * ADDRESSES_PER_SECTOR * SECTOR_SIZE)

/**
 * @brief read all inodes from disk and print out their content to
 *        stdout according to the assignment
 * @param u the filesystem
 * @return 0 on success; < 0 on error.
 */
int inode_scan_print(const struct unix_filesystem *u)
{
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(&u->s);

    size_t sectStart = u->s.s_inode_start;
    size_t nbrSect = u->s.s_isize;

    struct inode tempInodes[INODES_PER_SECTOR];

    size_t sectCount = 0;
    while (sectCount < nbrSect) {

        int readFeedback = sector_read(u->f, (uint32_t) (sectStart + sectCount), tempInodes);
        if (readFeedback != 0) {
            return readFeedback;
        }

        for(size_t i = 0; i < INODES_PER_SECTOR; ++i) {
            if (tempInodes[i].i_mode & IALLOC) {
                if (tempInodes[i].i_mode & IFDIR) {
                    //correcteur : Il aurait été possible de modulariser ces deux printf en un seul (utiliisation de l'opérateur ternaire ou bien variable intremédiaire pour le type de l'inode) (-0.5)
                    printf("inode   %lu (%s) len   %d\n", i + INODES_PER_SECTOR * sectCount,
                           SHORT_DIR_NAME, inode_getsize(&tempInodes[i]));
                } else {
                    printf("inode   %lu (%s) len   %d\n", i + INODES_PER_SECTOR * sectCount,
                           SHORT_FIL_NAME, inode_getsize(&tempInodes[i]));
                }
            }
            //correcteur : le fflush est inutile si votre printf se termine par un retour à la ligne (ce qui est votre cas ici)
            fflush(stdout);
        }
        sectCount += 1;
    }
    return 0;
}

/**
 * @brief prints the content of an inode structure
 * @param inode the inode structure to be displayed
 */
void inode_print(const struct inode *inode)
{
    if (inode != NULL) {
        printf("**********FS INODE START**********\n");
        printf("%-20s: %" PRIu16 "\n", "i_mode", inode->i_mode);
        printf("%-20s: %" PRIu8 "\n", "i_nlink", inode->i_nlink);
        printf("%-20s: %" PRIu8 "\n", "i_uid", inode->i_uid);
        printf("%-20s: %" PRIu8 "\n", "i_gid", inode->i_gid);
        printf("%-20s: %" PRIu8 "\n", "i_size0", inode->i_size0);
        printf("%-20s: %" PRIu16 "\n", "i_size1", inode->i_size1);
        printf("%-20s: %" PRIu32 "\n", "size", inode_getsize(inode));
        printf("**********FS INODE END**********\n");
    } else {
        //correcteur : même commentaire que dans print_superblock (-0.25)
        printf("**********FS INODE START**********\n");
        printf("NULL ptr");
        printf("**********FS INODE END**********\n");
    }
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
    M_REQUIRE_NON_NULL(&u->s);

    //correcteur : il aurait fallut tester deux choses en plus : si le numéro était plus petit que 1 (l'inode ne pouvant pas être accédée), et que le poitneur d'inode ne soit pas NULL (-1.5)

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
 * @return >0: the sector on disk; <0 error
 */
int inode_findsector(const struct unix_filesystem *u, const struct inode *i, int32_t file_sec_off)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    //correcteur : je le dis une fois ici (parce que vous le faites souvent tout le long du projet) mais ça n'a pas de sens de tester la nullitude d'un objet dont vous obtenez la référence (forcément cette valeur ne sera jamais null) (1 point de malus)
    M_REQUIRE_NON_NULL(&u->s);
    M_REQUIRE_NON_NULL(i);

    if(i->i_mode & IALLOC) {
        int32_t inodeSize = inode_getsize(i);
        //correcteur : bonne utilisation des macros ! (0.5 point bonus)
        if(inodeSize > SECT_UP_LIM) {
            // ERR FIL TOO LARGE
            return ERR_FILE_TOO_LARGE;
        } else {
            if(inodeSize > SECT_DOWN_LIM) {
                // FIL MID-SIZE
                //correcteur : vu la nature de vos données, des uint32_t auraient été plus appropriés.
                int32_t offsetIAddr = file_sec_off / ADDRESSES_PER_SECTOR;
                int32_t indirectOffset = file_sec_off % ADDRESSES_PER_SECTOR;

                if(offsetIAddr >= 0 && offsetIAddr < ADDR_SMALL_LENGTH) {
                    //correcteur : pourquoi le +1 ? Vous n'allez pas imprimer ce tableau, donc pas besoin de place pour une terminaison de chaîne de caractère (-1)
                    uint16_t temp[ADDRESSES_PER_SECTOR + 1];

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

                if(file_sec_off >= 0 && file_sec_off < ADDR_SMALL_LENGTH) {
                    return i->i_addr[file_sec_off];
                }

                return ERR_OFFSET_OUT_OF_RANGE;
            }
        }
    }

    return ERR_UNALLOCATED_INODE;
}
