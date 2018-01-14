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
 * @return 0 on success; <0 on error
 */
int filev6_lseek(struct filev6 *fv6, int32_t offset)
{
    M_REQUIRE_NON_NULL(fv6);

    if (0 <= offset && offset < inode_getsize(&fv6->i_node)) {
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
 * @param mode the mode of the file
 * @param fv6 the filev6 (IN-OUT; offset will be changed)
 * @return 0 on success; <0 on error
 */
int filev6_create(struct unix_filesystem *u, uint16_t mode, struct filev6 *fv6)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(fv6);

    struct inode tempInode;
    memset(&tempInode, 0, sizeof(struct inode));
    tempInode.i_mode = mode;

    int err = inode_write(u, fv6->i_number, &tempInode);
    if (!err) {
        memcpy(&fv6->i_node, &tempInode, sizeof(struct inode));
        fv6->offset = 0;
        return 0;
    }
    return err;
}

/**
 * @brief write a sector (the given buffer) on disk to the given filev6 at a given offset
 * @param u the filesystem (IN)
 * @param fv6 the filev6 (IN)
 * @param buf the data we want to write (IN)
 * @param offset
 * @return 0 on success; <0 on error
 */
int filev6_writesector(struct unix_filesystem *u, struct filev6 *fv6, const void *buf, uint32_t sector)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(fv6);
    M_REQUIRE_NON_NULL(buf);

    int feedback = sector_write(u->f, sector, buf);
    if (feedback == 0) {
        bm_set(u->fbm, sector);
    }

    return feedback;
}

/**
 * @brief write the len bytes of the given buffer on disk to the given filev6
 * @param u the filesystem (IN)
 * @param fv6 the filev6 (IN)
 * @param buf the data we want to write (IN)
 * @param len the length of the bytes we want to write
 * @return 0 on success; <0 on error
 */
int filev6_writebytes(struct unix_filesystem *u, struct filev6 *fv6, const void *buf, int len)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(fv6);
    M_REQUIRE_NON_NULL(buf);

    if (len < 0) {
        return ERR_BAD_PARAMETER;
    }

    int32_t inode_size = inode_getsize(&fv6->i_node);
    uint32_t offset = 0;

    if (inode_size + len > SECT_UP_LIM) {
        return ERR_FILE_TOO_LARGE;
    }

    uint32_t free_space = (uint32_t) (SECTOR_SIZE - inode_size % SECTOR_SIZE);
    int err = 0;

    if ((free_space > 0) && (free_space != SECTOR_SIZE)) {
        /// 1. There is space left in the last sector, we fill it before creating new sectors.

        // sect_buf will contain the concatenation of the existing content and what we add.
        char sect_buf[SECTOR_SIZE];
        memset(sect_buf, 0, SECTOR_SIZE);

        uint32_t to_add = (uint32_t) len < free_space ? (uint32_t) len : free_space;

        int file_sec_off = (inode_getsectorsize(&fv6->i_node) - 1) / SECTOR_SIZE - 1;
        if (file_sec_off < 0) {
            file_sec_off = 0;
        }

        int sector = inode_findsector(u, &fv6->i_node, file_sec_off);
        if (sector < 0) {
            return sector;
        }

        err = sector_read(u->f, (uint32_t) sector, sect_buf);
        if (err != 0) {
            return err;
        }
        memcpy(sect_buf + (SECTOR_SIZE - free_space), buf, to_add);

        err = filev6_writesector(u, fv6, sect_buf, (uint32_t) sector);
        if (err != 0) {
            return err;
        }

        // We increase the offset for the data to write.
        offset += to_add;
    }

    /// 2. We maybe have to allocate new sectors and write in them.
    while (offset < (uint32_t) len) {
        uint32_t nb_bytes = ((uint32_t) len - offset >= SECTOR_SIZE) ? SECTOR_SIZE : ((uint32_t) len - offset);

        /// 2.1 We write at the next available place if it exists and write it to the disk.
        int sec_content = bm_find_next(u->fbm);
        if (sec_content < 0) {
            return sec_content;
        }

        err = filev6_writesector(u, fv6, (const char *) buf + offset, (uint32_t) sec_content);
        if (err != 0) {
            return err;
        }

        /// 2.2  Update of i_addr
        int small = ((uint32_t)inode_size + offset + nb_bytes <= SECT_DOWN_LIM);
        int medium = ((uint32_t)inode_size + offset + nb_bytes > SECT_DOWN_LIM);
        int goingFromSmallToMedium = (((uint32_t)inode_size + offset <= SECT_DOWN_LIM) &&
                                      ((uint32_t)inode_size + offset + nb_bytes > SECT_DOWN_LIM));

        if (small) { /// 2.2.1 Direct sectors
            int32_t nb_sect_used = (int32_t)((uint32_t)inode_size + offset ) > 0 ?
                                   (int32_t)((((uint32_t)inode_size + offset - 1) / SECTOR_SIZE) + 1) : 0;
            fv6->i_node.i_addr[nb_sect_used] = (uint16_t) sec_content;
        }
        if(goingFromSmallToMedium) { /// 2.2.2 From direct to indirect sectors.
            /// 2.2.2.1 Gathering old sectors into one new and linking it to iaddr.
            /// (here we are not adding the address of the new sector)
            uint16_t temp_sect_addr[ADDRESSES_PER_SECTOR];
            memset(temp_sect_addr, 0, ADDRESSES_PER_SECTOR);

            int32_t nb_sect_used = (int32_t)((uint32_t)inode_size + offset ) > 0 ?
                                   (int32_t)((((uint32_t)inode_size + offset - 1) / SECTOR_SIZE) + 1) : 0;
            for (int32_t i = 0; i < nb_sect_used; ++i) {
                temp_sect_addr[i] = (uint16_t) fv6->i_node.i_addr[i];
            }

            int new_indirect_sect = bm_find_next(u->fbm);
            if (new_indirect_sect < 0) {
                return new_indirect_sect;
            }

            err = sector_write(u->f, (uint32_t) new_indirect_sect, temp_sect_addr);
            if (err < 0) {
                return err;
            }

            memset(fv6->i_node.i_addr, 0, sizeof(fv6->i_node.i_addr));
            fv6->i_node.i_addr[0] = (uint16_t) new_indirect_sect;
            bm_set(u->fbm, (uint64_t) new_indirect_sect);
        }

        if (medium) { /// 2.2.3 Indirect sectors
            uint16_t temp_sect_addr[ADDRESSES_PER_SECTOR];
            memset(temp_sect_addr, 0, sizeof(fv6->i_node.i_addr));

            int32_t nb_sect_used = (int32_t)((uint32_t)inode_size + offset ) > 0 ?
                                   (int32_t)((((uint32_t)inode_size + offset - 1) / SECTOR_SIZE) + 1) : 0;
            int32_t i_addr_off = nb_sect_used > 0 ? ((nb_sect_used - 1) / ADDRESSES_PER_SECTOR) : 0;

            int lastIndirectSectIsFull = ((nb_sect_used % ADDRESSES_PER_SECTOR) == 0);
            if (lastIndirectSectIsFull ) { /// 2.2.3.1 We need to allocate one new sector.
                i_addr_off++;

                int new_indirect_sect = bm_find_next(u->fbm);
                if (new_indirect_sect < 0) {
                    return new_indirect_sect;
                }

                temp_sect_addr[0] = (uint16_t) sec_content;

                err = filev6_writesector(u,fv6, temp_sect_addr, (uint32_t)new_indirect_sect);
                if (err < 0) {
                    return err;
                }

                fv6->i_node.i_addr[i_addr_off] = (uint16_t)new_indirect_sect;

            } else { /// 2.2.3.2 We can add our address in the last indirect sector.
                err = sector_read(u->f, fv6->i_node.i_addr[i_addr_off], temp_sect_addr);
                if (err < 0) {
                    return err;
                }

                temp_sect_addr[nb_sect_used % ADDRESSES_PER_SECTOR] = (uint16_t) sec_content;

                err = sector_write(u->f, fv6->i_node.i_addr[i_addr_off], temp_sect_addr);
                if (err < 0) {
                    return err;
                }
            }
        }

        /// 2.3 we set to 1 sect we used.
        bm_set(u->fbm, (uint64_t) sec_content);

        /// 2.4 We increase the offset for the data to write.
        offset += nb_bytes;
    }

    err = inode_setsize(&fv6->i_node, inode_size + len);
    if (err != 0) {
        return err;
    }

    err = inode_write(u, fv6->i_number, &fv6->i_node);
    if (err != 0) {
        return err;
    }

    return 0;
}
