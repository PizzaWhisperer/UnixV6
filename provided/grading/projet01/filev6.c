#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inode.h"
#include "error.h"
#include "sector.h"
#include "filev6.h"

/**
 * @brief open up a file corresponding to a given inode; set offset to zero
 * @param u the filesystem (IN)
 * @param inr he inode number (IN)
 * @param fv6 the complete filve6 data structure (OUT)
 * @return 0 on success; <0 on errror
 */
int filev6_open(const struct unix_filesystem *u, uint16_t inr, struct filev6 *fv6)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(fv6);

    fv6->u = u;
    fv6->i_number = inr;
    fv6->offset = 0;

    return inode_read(fv6->u, fv6->i_number, &fv6->i_node);
}

/**
 * @brief change the current offset of the given file to the one specified
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @param off the new offset of the file
 * @return 0 on success; <0 on errror
 */
int filev6_lseek(struct filev6 *fv6, int32_t offset);

/**
 * @brief read at most SECTOR_SIZE from the file at the current cursor
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @param buf points to SECTOR_SIZE bytes of available memory (OUT)
 * @return >0: the number of bytes of the file read; 0: end of file; <0 error
 */
int filev6_readblock(struct filev6 *fv6, void *buf)
{
    M_REQUIRE_NON_NULL(fv6);
    M_REQUIRE_NON_NULL(fv6->u);
    //correcteur : cf malus mis dans inode
    M_REQUIRE_NON_NULL(&fv6->u->s);
    M_REQUIRE_NON_NULL(fv6->u->f);

    int inodeSize = inode_getsize(&fv6->i_node);
    if(inodeSize <= fv6->offset) {

        return 0;
    } else {
        int sect = inode_findsector(fv6->u, &(fv6->i_node), fv6->offset / SECTOR_SIZE);

        if (sect < 0) {
            return sect;
        } else {

            int read = sector_read(fv6->u->f, (uint32_t) sect, buf);
            if(read < 0) {
                return read;
            }

            int readBytes = (inodeSize - fv6->offset >= SECTOR_SIZE) ? SECTOR_SIZE : inodeSize - fv6->offset;
            fv6->offset += readBytes;

            return readBytes;
        }
    }
}

/**
 * @brief create a new filev6
 * @param u the filesystem (IN)
 * @param mode the new offset of the file
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @return 0 on success; <0 on errror
 */
int filev6_create(struct unix_filesystem *u, uint16_t mode, struct filev6 *fv6);

/**
 * @brief write the len bytes of the given buffer on disk to the given filev6
 * @param u the filesystem (IN)
 * @param fv6 the filev6 (IN)
 * @param buf the data we want to write (IN)
 * @param len the length of the bytes we want to write
 * @return 0 on success; <0 on errror
 */
int filev6_writebytes(struct unix_filesystem *u, struct filev6 *fv6, void *buf, int len);
