/**
 * @file direntv6.c
 * @brief accessing the UNIX v6 filesystem -- directory layer
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "error.h"
#include "direntv6.h"
#include "unixv6fs.h"
#include "bmblock.h"
#include "inode.h"
#include "filev6.h"

#define PATH_TOKEN_STRING "/"

/**
 * @brief opens a directory reader for the specified inode 'inr'
 * @param u the mounted filesystem
 * @param inr the inode -- which must point to an allocated directory
 * @param d the directory reader (OUT)
 * @return 0 on success; <0 on error
 */
int direntv6_opendir(const struct unix_filesystem *u, uint16_t inr, struct directory_reader *d)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(d);
    M_REQUIRE_NON_NULL(u->f);

    if (inr < 1) {
        return ERR_BAD_PARAMETER;
    }

    int createFeedback = filev6_open(u, inr, &d->fv6);
    if (createFeedback != 0) {
        return createFeedback;
    }

    d->last = 0;
    d->cur = 0;

    if (d->fv6.i_node.i_mode & IALLOC) {
        if ((d->fv6.i_node.i_mode & IFMT) == IFDIR) {
            return 0;
        }

        return ERR_INVALID_DIRECTORY_INODE;
    }

    return ERR_UNALLOCATED_INODE;
}

/**
 * @brief return the next directory entry.
 * @param d the directory reader
 * @param name pointer to at least DIRENT_MAXLEN + 1 bytes.  Filled in with the NULL-terminated string of the entry (OUT)
 * @param child_inr pointer to the inode number in the entry (OUT)
 * @return 1 on success;  0 if there are no more entries to read; <0 on error
 */
int direntv6_readdir(struct directory_reader *d, char *name, uint16_t *child_inr)
{
    M_REQUIRE_NON_NULL(d);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(child_inr);

    if (d->cur == d->last) { // We need to read the next block.
        struct direntv6 temp[SECTOR_SIZE];
        memset(temp, 0, SECTOR_SIZE* sizeof(struct direntv6));

        int readFeedback = filev6_readblock(&d->fv6, temp);
        if (readFeedback < 1) {
            return readFeedback;
        } else {
            memcpy(d->dirs, temp, DIRENTRIES_PER_SECTOR * sizeof(struct direntv6));
            d->last += (uint32_t) ((size_t) readFeedback / sizeof(struct direntv6));
        }
    }
    *child_inr = d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_inumber;

    memset(name, 0, DIRENT_MAXLEN + 1);
    strncpy(name, d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_name, DIRENT_MAXLEN);
    name[DIRENT_MAXLEN] = '\0';

    d->cur += 1;

    return 1;
}

/**
* @brief debugging routine; print a subtree (note: recursive)
* @param u a mounted filesystem
* @param inr the root of the subtree
* @param prefix the prefix to the subtree
* @return 0 on success; <0 on error
*/
int direntv6_print_tree(const struct unix_filesystem *u, uint16_t inr, const char *prefix)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(prefix);

    struct directory_reader d;
    memset(&d, 0, sizeof(struct directory_reader));

    int feedback = direntv6_opendir(u, inr, &d);
    if (feedback < 0) {

        if (feedback != ERR_INVALID_DIRECTORY_INODE) {
            // ERR
            return feedback;
        } else {
            // FIL
            printf("%s ", SHORT_FIL_NAME);
            printf("%s\n", prefix);
            fflush(stdout);
        }
    } else if (feedback == 0) {
        // DIR
        printf("%s ", SHORT_DIR_NAME);
        printf("%s%c\n", prefix, PATH_TOKEN);
        fflush(stdout);

        struct direntv6 child;
        memset(&child, 0, sizeof(struct direntv6));
        int childFeedback = 1;

        char tempName[DIRENT_MAXLEN + 1];
        memset(tempName, 0, DIRENT_MAXLEN + 1);
        tempName[0] = '\0';

        while ((childFeedback = direntv6_readdir(&d, tempName, &child.d_inumber)) > 0) {
            strncpy(child.d_name, tempName, DIRENT_MAXLEN);

            char nextPref[MAXPATHLEN_UV6 + 1];
            memset(nextPref, 0, MAXPATHLEN_UV6 + 1);

            strncpy(nextPref, prefix, strlen(prefix));
            nextPref[strlen(prefix)] = '\0';

            if (strlen(nextPref) < MAXPATHLEN_UV6) {
                if (feedback != ERR_INVALID_DIRECTORY_INODE) {
                    nextPref[strlen(nextPref)] = PATH_TOKEN;
                }

                size_t min = DIRENT_MAXLEN < (MAXPATHLEN_UV6 - strlen(nextPref)) ?
                             DIRENT_MAXLEN : (MAXPATHLEN_UV6 - strlen(nextPref));

                strncat(nextPref, child.d_name, min);
                nextPref[strlen(nextPref)] = '\0';
            }

            int printFeedback = direntv6_print_tree(u, child.d_inumber, nextPref);
            if (printFeedback != 0) {
                return printFeedback;
            }
        }
        if (childFeedback < 0) {
            return childFeedback;
        }
    }

    return 0;
}

/**
* @brief get the inode number for the given path
* @param u a mounted filesystem
* @param inr the root of the subtree
* @param entry pathname relative to the subtree
* @param size of the entry
* @return inr on success; <0 on error
*/
int direntv6_dirlookup_core(const struct unix_filesystem *u, uint16_t inr, const char *entry, size_t size)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(entry);

    if (size <= 0) {
        return ERR_INODE_OUTOF_RANGE;
    } else {

        char entryTempCpy[MAXPATHLEN_UV6 + 1];
        memset(entryTempCpy, 0, MAXPATHLEN_UV6 + 1);
        strncpy(entryTempCpy, entry, strlen(entry) + 1);
        entryTempCpy[MAXPATHLEN_UV6] = '\0';

        char* tmp = trim_slash(entryTempCpy);
        char entry_cpy[MAXPATHLEN_UV6 + 1];
        memset(entry_cpy, 0, MAXPATHLEN_UV6 + 1);
        strncpy(entry_cpy, tmp, strlen(tmp) + 1);
        entry_cpy[MAXPATHLEN_UV6] = '\0';

        char* p = strchr(entry_cpy, PATH_TOKEN);

        struct directory_reader d;
        char name[DIRENT_MAXLEN + 1];
        uint16_t child_inr = 0;

        direntv6_opendir(u, inr, &d);
        name[DIRENT_MAXLEN] = '\0';

        int err = 0;
        if (p == NULL) { // At the end of the iteration.
            while ((err = direntv6_readdir(&d, name, &child_inr)) > 0) {
                if (strcmp(name, entry_cpy) == 0) {
                    return child_inr;
                }

                memset(name, 0, DIRENT_MAXLEN);
                child_inr = 0;
            }

            return ERR_INODE_OUTOF_RANGE;
        } else { // We need to find the right directory to explore.

            *p++ = '\0';

            while ((err = direntv6_readdir(&d, name, &child_inr)) > 0) {
                if (strcmp(name, entry_cpy) == 0) {
                    char p_cpy[strlen(p) + 1];
                    memcpy(p_cpy, p, strlen(p));
                    p_cpy[strlen(p)] = '\0';

                    return direntv6_dirlookup_core(u, child_inr, p_cpy, strlen(p_cpy));
                }
                memset(name, 0, DIRENT_MAXLEN);
            }
            if (err < 0) {
                return err;
            }
        }
    }

    return ERR_INODE_OUTOF_RANGE;
}
/**
 * @brief get the inode number for the given path
 * @param u a mounted filesystem
 * @param inr the root of the subtree
 * @param entry pathname relative to the subtree
 * @return inr on success; <0 on error
 */
int direntv6_dirlookup(const struct unix_filesystem *u, uint16_t inr, const char *entry)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(entry);

    if (strcmp(entry, PATH_TOKEN_STRING) == 0) {
        struct directory_reader d;
        memset(&d, 0, sizeof(struct directory_reader));

        if (direntv6_opendir(u, inr, &d) == 0) {
            return inr;
        }
    }

    return direntv6_dirlookup_core(u, inr, entry, strlen(entry));
}

/**
 * @brief skip the '/' in the front of a string
 * @param str pointer to the string
 * @return str increased to skip the '/' in the front
 */
char* trim_slash(char *str)
{
    if (str == NULL) {
        return NULL;
    }

    size_t i = 0;
    while (str[i] == PATH_TOKEN) {
        i += 1;
    }

    return str + i;
}

/**
 * @brief create a new direntv6 with the given name and given mode
 * @param u a mounted filesystem
 * @param entry the path of the new entry
 * @param mode the mode of the new inode
 * @return inr on success; <0 on error
 */
int direntv6_create(struct unix_filesystem *u, const char *entry, uint16_t mode)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(entry);

    // Separate PARENT and CHILD.
    char entry_cpy[MAXPATHLEN_UV6 + 1];
    memset(entry_cpy, 0, MAXPATHLEN_UV6 + 1);
    strncpy(entry_cpy, entry, strlen(entry) + 1);
    entry_cpy[MAXPATHLEN_UV6] = '\0';

    const char* parent = trim_slash(entry_cpy);
    char* child = strrchr(entry_cpy, PATH_TOKEN);

    while ((child - 1) >= parent && *(child - 1) == PATH_TOKEN) {
        child -= 1;
    }
    *child = '\0';
    child += 1;
    child = trim_slash(child);
    if (strlen(child) > DIRENT_MAXLEN) {
        return ERR_FILENAME_TOO_LONG;
    }

    // If parent == child, then parent has to be the root dir "/".
    if (parent == child) {
        parent = PATH_TOKEN_STRING; // Since parent is "const char*", this is allowed.
    }

    // Does parent exist and child not?
    int parentInodeNumber = direntv6_dirlookup(u, ROOT_INUMBER, parent);
    if (parentInodeNumber < 0) {
        return ERR_BAD_PARAMETER;
    }

    int feedback = direntv6_dirlookup(u, (uint16_t) parentInodeNumber, child);
    if (feedback >= 0) {
        return ERR_FILENAME_ALREADY_EXISTS;
    }

    // Get the next free inode and write there.
    int childInodeNumber = inode_alloc(u);
    if (childInodeNumber < 0) {
        return childInodeNumber;
    }

    // Creation of the direntv6 (child) with the inode number and the entry name.
    struct direntv6 childDirentv6;
    memset(&childDirentv6, 0, sizeof(struct direntv6));
    childDirentv6.d_inumber = (uint16_t) childInodeNumber;
    size_t toCopy = strlen(child) < DIRENT_MAXLEN ? strlen(child) + 1 : DIRENT_MAXLEN;
    memcpy(childDirentv6.d_name, child, toCopy);

    // Creation of the fv6 (parent) with the unix file system, the inode number and the corresponding inode (child).
    struct filev6 parentFv6;
    memset(&parentFv6, 0, sizeof(struct filev6));
    feedback = filev6_open(u, (uint16_t) parentInodeNumber, &parentFv6);
    if (feedback != 0) {
        return feedback;
    }

    // We write the content to the disk.
    feedback = filev6_writebytes(u, &parentFv6, &childDirentv6, sizeof(struct direntv6));
    if (feedback != 0) {
        return feedback;
    }

    // Write the inode
    struct inode tempInode;
    memset(&tempInode, 0, sizeof(struct inode));
    tempInode.i_mode = mode | IALLOC;

    int32_t inodeParentSize = inode_getsize(&parentFv6.i_node);
    int32_t offset = inodeParentSize > 0 ? ((inodeParentSize - 1) / SECTOR_SIZE) + 1 : 0;

    int sector = inode_findsector(u, &parentFv6.i_node, offset);
    if (sector < 0) {
        return sector;
    }
    tempInode.i_addr[0] = (uint16_t) sector;

    feedback = inode_write(u, (uint16_t) childInodeNumber, &tempInode);
    if (feedback != 0) {
        bm_clear(u->ibm, (uint64_t) childInodeNumber);
        return feedback;
    }

    return childInodeNumber;
}
