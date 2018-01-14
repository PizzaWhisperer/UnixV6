#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inode.h"
#include "error.h"
#include "sector.h"
#include "filev6.h"

/**
 * @brief open the file corresponding to a given inode; set offset to zero
 * @param u the filesystem (IN)
 * @param inr the inode number (IN)
 * @param fv6 the complete filve6 data structure (OUT)
 * @return 0 on success; the appropriate error code (<0) on error
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
int filev6_lseek(struct filev6 *fv6, int32_t offset)
{
    M_REQUIRE_NON_NULL(fv6);
    //correcteur : le lseek doit être fait sur le file de votre filesystem, pas besoin de le faire sur le disk également ! (-0.5)
    if ((0 <= offset && offset < inode_getsize(&fv6->i_node)) && fseek(fv6->u->f, offset, SEEK_SET) == 0) {
        fv6->offset = offset;

        return 0;
    }

    return ERR_OFFSET_OUT_OF_RANGE;
}

/**
 * @brief read at most SECTOR_SIZE from the file at the current cursor
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @param buf points to SECTOR_SIZE bytes of available memory (OUT)
 * @return >0: the number of bytes of the file read; 0: end of file;
 *             the appropriate error code (<0) on error
 */
int filev6_readblock(struct filev6 *fv6, void *buf)
{
    M_REQUIRE_NON_NULL(fv6);
    M_REQUIRE_NON_NULL(fv6->u);
    M_REQUIRE_NON_NULL(&fv6->u->s);
    M_REQUIRE_NON_NULL(fv6->u->f);
    M_REQUIRE_NON_NULL(buf);

    int inodeSize = inode_getsize(&fv6->i_node);
    if (inodeSize <= fv6->offset) {

        return 0;
    } else {
        int sect = inode_findsector(fv6->u, &(fv6->i_node), fv6->offset / SECTOR_SIZE);

        if (sect < 0) {
            return sect;
        } else {

            int read = sector_read(fv6->u->f, (uint32_t) sect, buf);
            if (read < 0) {
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
int filev6_create(struct unix_filesystem *u, uint16_t mode, struct filev6 *fv6)
{
    /// METTRE OFFSET DANS FV6 A ZERO???
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(fv6);

    struct inode tempInode;
    memset(&tempInode, 0, sizeof(struct inode));
    tempInode.i_mode = mode;

    int err = inode_write(u, fv6->i_number, &tempInode);
    if (!err) {
        memcpy(&fv6->i_node, &tempInode, sizeof(struct inode));

        return 0;
    }

    return err;
}

/**
 * @brief write the len bytes of the given buffer on disk to the given filev6
 * @param u the filesystem (IN)
 * @param fv6 the filev6 (IN)
 * @param buf the data we want to write (IN)
 * @param len the length of the bytes we want to write
 * @return 0 on success; <0 on errror
 */
int filev6_writebytes(struct unix_filesystem *u, struct filev6 *fv6, const void *buf, int len)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(fv6);
    M_REQUIRE_NON_NULL(buf);

    (void) len;

    /// TODO (just printf)
    return 0;
}
