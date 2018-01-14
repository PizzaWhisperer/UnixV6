#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "mount.h"
#include "error.h"
#include "sector.h"

#define BYTE_SIZE 8
#define NAMES_LENGTH 14

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
    u->fbm = NULL;
    u->ibm = NULL;

    u->f = fopen(filename, "r");
    if (u->f == NULL) {
        return ERR_IO;
    }
    //correcteur : puisque vous en aviez la possibilité il aurait été mieux d'utiliser uint8_t ici.
    unsigned char temp[SECTOR_SIZE];

    int readFeedback = sector_read(u->f, BOOTBLOCK_SECTOR, temp);
    if (readFeedback != 0) {
        return readFeedback;
    }

    if (temp[BOOTBLOCK_MAGIC_NUM_OFFSET] != BOOTBLOCK_MAGIC_NUM) {
        return ERR_BADBOOTSECTOR;
    }

    readFeedback = sector_read(u->f, SUPERBLOCK_SECTOR, temp);
    if (readFeedback != 0) {
        return readFeedback;
    }

    memcpy(&u->s, temp, SECTOR_SIZE);

    return 0;
}

/**
 * @brief print to stdout the content of the superblock
 * @param u - the mounted filesytem
 */
void mountv6_print_superblock(const struct unix_filesystem *u)
{
    if (u != NULL) {
        printf("**********FS SUPERBLOCK START**********\n");
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
        printf("**********FS SUPERBLOCK END**********\n");
    } else {
        //correcteur : manque de modularité vous auriez du mettre les deux prints de start et end en dehors du if-else (-0.25)
        printf("**********FS SUPERBLOCK START**********\n");
        printf("NULL ptr");
        printf("**********FS SUPERBLOCK END**********\n");
    }
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

    if (!ferror(u->f) || fclose(u->f) != 0) {
        return ERR_IO;
    }

    return 0;
}
