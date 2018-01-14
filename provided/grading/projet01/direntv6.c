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

    if(d->cur == d->last) {
        //correcteur : une solution plus facile aurait été de déclarer ce tableau comme un tableau de direntries.
        unsigned char temp[SECTOR_SIZE];
        memset(temp, 0, SECTOR_SIZE);

        int readFeedback = filev6_readblock(&d->fv6, temp);
        if(readFeedback == 0) {
            return 0;
        }
        //correcteur : vous auriez pu directement tester readFeedback < 1 ici au lieu de tester à part readFeedback == 0 (-0.5)
        if(readFeedback < 0) {
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

        while((childFeedback = direntv6_readdir(&d, tempName, &child.d_inumber)) > 0) {
            strncpy(child.d_name, tempName, DIRENT_MAXLEN);
            //correcteur : Cette partie du code pourrait être rendue plus facile et moins verbeuse en utilisant astucieusement la fonction snprintf et un bon formatteur (je vous laisse aller regarder la manpage correspondante si vous souhaitez réduire votre nombre de ligne de code)
            char nextPref[MAXPATHLEN_UV6 + 1];
            memset(nextPref, 0, MAXPATHLEN_UV6 + 1);

            strcpy(nextPref, prefix);
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
* @param inr the current of the subtree
* @param entry the prefix to the subtree
* @param size of the entry
* @return inr on success; <0 on error
*/
int direntv6_dirlookup_core(const struct unix_filesystem *u, uint16_t inr, const char *entry, size_t size)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(entry);
    //correcteur : ce check n'est pas nécessaire, il est déjà fait dans filev6_open qui est appelé dans opendir que vous utilisez plus bas (-1)
    if(size <= 0) {
        return ERR_INODE_OUTOF_RANGE;
    } else {

        char entry_cpy[MAXPATHLEN_UV6 + 1];
        memcpy(entry_cpy, entry, MAXPATHLEN_UV6);
        entry_cpy[MAXPATHLEN_UV6] = '\0';
        strcpy(entry_cpy, trim_slash(entry_cpy));

        char* p = strchr(entry_cpy, PATH_TOKEN);

        struct directory_reader d;
        char name[DIRENT_MAXLEN + 1];
        uint16_t child_inr = 0;

        direntv6_opendir(u, inr, &d);
        name[DIRENT_MAXLEN] = '\0';

        int err = 0;
        if(p == NULL) { // At the end of the iteration.
            while((err = direntv6_readdir(&d, name, &child_inr)) > 0) {
                if(strcmp(name, entry_cpy) == 0) {
                    return child_inr;
                }

                memset(name, 0, DIRENT_MAXLEN);
                child_inr = 0;
            }

            return ERR_INODE_OUTOF_RANGE;
        } else { // We need to find the right directory to explore

            *p++ = '\0';

            while((err = direntv6_readdir(&d, name, &child_inr)) > 0) {
                if(strcmp(name, entry_cpy) == 0) {
                    char p_cpy[strlen(p) + 1];
                    memcpy(p_cpy, p, strlen(p));
                    p_cpy[strlen(p)] = '\0';

                    return direntv6_dirlookup_core(u, child_inr, p_cpy, strlen(p_cpy));
                }
                memset(name, 0, DIRENT_MAXLEN);
            }
            //correcteur : et si err < 0 vous ne propagez pas l'erreur (-2)
        }
    }

    return ERR_INODE_OUTOF_RANGE;
}
/**
 * @brief get the inode number for the given path
 * @param u a mounted filesystem
 * @param inr the current of the subtree
 * @param entry the prefix to the subtree
 * @return inr on success; <0 on error
 */
int direntv6_dirlookup(const struct unix_filesystem *u, uint16_t inr, const char *entry)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(u->f);
    M_REQUIRE_NON_NULL(entry);

    return direntv6_dirlookup_core(u, inr, entry, strlen(entry));
}
//correcteur : pensez à déclarer en static vos méthodes à usage privé.
char * trim_slash(char *str)
{
    int i = 0;
    while(str[i] == PATH_TOKEN) {
        i += 1;
    }
    str += i;

    return str;
}
