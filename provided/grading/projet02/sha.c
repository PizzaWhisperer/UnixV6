#include <stdio.h>
#include <openssl/sha.h>
#include <string.h>
#include "unixv6fs.h"
#include "sha.h"
#include "inode.h"
#include "sector.h"
#include "error.h"
#include "filev6.h"

static void sha_to_string(const unsigned char *SHA, char *sha_string)
{
    if (SHA != NULL && sha_string != NULL) {
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            sprintf(&sha_string[i * 2], "%02x", SHA[i]);
        }

        sha_string[2 * SHA256_DIGEST_LENGTH] =  '\0' ;
    }
}

/**
 * @brief print the sha of the content
 * @param content the content of which we want to print the sha
 * @param length the length of the content
 */
void print_sha_from_content(const unsigned char *content, size_t length)
{
    if (content != NULL) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        char shaString[2 * SHA256_DIGEST_LENGTH + 1];

        SHA256(content, length, hash);
        sha_to_string(hash, shaString);
        printf("%s\n", shaString);
    }
}

/**
 * @brief print the sha of the content of an inode
 * @param u the filesystem
 * @param inode the inocde of which we want to print the content
 * @param inr the inode number
 */
void print_sha_inode(struct unix_filesystem *u, struct inode inode, int inr)
{
    if (u != NULL) {
        if (inode.i_mode & IALLOC) {

            printf("SHA inode %d: ", inr);
            if (inode.i_mode & IFDIR) {

                printf("no SHA for directories.\n");
            } else {

                int32_t inodeSize = inode_getsize(&inode);
                char content[inodeSize + 1];
                memset(content, 0, (size_t) (inodeSize + 1));
                content[0] = '\0';
                content[inodeSize] = '\0';

                unsigned char buffer[SECTOR_SIZE + 1];
                memset(buffer, 0, SECTOR_SIZE + 1);
                buffer[SECTOR_SIZE] = '\0';

                for (int32_t offset = 0; offset <= inodeSize / SECTOR_SIZE; ++offset) {

                    int sector = inode_findsector(u, &inode, offset);
                    if (sector < 0) {
                        return;
                    }
                    memset(buffer, 0, SECTOR_SIZE);

                    int j = sector_read(u->f, (uint32_t) sector, buffer);
                    if (j < 0) {
                        return;
                    }

                    strncat(content, (const char*) buffer, strlen((const char*) buffer));
                }

                print_sha_from_content((const unsigned char*) content, strlen((const char*) content));
            }
        }

        fflush(stdout);
    }
}
