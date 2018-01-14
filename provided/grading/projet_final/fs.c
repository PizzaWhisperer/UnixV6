/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION (26)

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "error.h"
#include "direntv6.h"
#include "unixv6fs.h"
#include "mount.h"
#include "filev6.h"
#include "sector.h"
#include "inode.h"

#define BLOCK_512B (512)

static struct unix_filesystem fs;

/*
 * From https://github.com/libfuse/libfuse/wiki/Option-Parsing.
 * This will look up into the args to search for the name of the FS.
 */
static int arg_parse(void* data, const char* filename, int key, struct fuse_args* outargs)
{
    (void) data;
    (void) outargs;

    if (key == FUSE_OPT_KEY_NONOPT && fs.f == NULL && filename != NULL) {
        int feedback = mountv6(filename, &fs);
        if (feedback) {
            fprintf(stderr, "ERROR: %d in arg_parse\n", feedback);
            exit(1);
        }

        return 0;
    }

    return 1;
}

static int fs_getattr(const char* path, struct stat* stbuf)
{
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(stbuf);

    memset(stbuf, 0, sizeof(struct stat));

    int inr = direntv6_dirlookup(&fs, ROOT_INUMBER, path);
    if (inr < 0) {
        return inr;
    }

    struct inode inode;
    int feedback = inode_read(&fs, (uint16_t) inr, &inode);
    if (feedback < 0) {
        return feedback;
    }

    if (inode.i_mode & IALLOC) {
        stbuf->st_ino = (ino_t) inr;
        stbuf->st_size = (off_t) inode_getsize(&inode);
        stbuf->st_blksize = (blksize_t) SECTOR_SIZE;

        stbuf->st_blocks = (blkcnt_t) (stbuf->st_size / BLOCK_512B);
        if (stbuf->st_size % BLOCK_512B != 0) {
            stbuf->st_blocks += 1;
        }

        stbuf->st_mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        if ((inode.i_mode & IFMT) == IFDIR) {
            stbuf->st_mode = stbuf->st_mode | S_IFDIR;
        } else {
            stbuf->st_mode = stbuf->st_mode | S_IFREG;
        }
    } else {
        return ERR_UNALLOCATED_INODE;
    }

    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    M_REQUIRE_NON_NULL(fs.f);
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(buf);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    struct directory_reader d;
    char name[DIRENT_MAXLEN + 1];
    memset(name, 0, DIRENT_MAXLEN + 1);
    name[DIRENT_MAXLEN] = '\0';
    uint16_t child_inr = 0;

    int inr = direntv6_dirlookup(&fs, ROOT_INUMBER, path);
    if (inr < 0) {
        return inr;
    }

    int err = direntv6_opendir(&fs, (uint16_t) inr, &d);
    if (err < 0) {
        return err;
    }

    while ((err = direntv6_readdir(&d, name, &child_inr)) > 0) {
        filler(buf, name, NULL, 0);
    }
    if (err < 0) {
        return err;
    }

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) fi;

    M_REQUIRE_NON_NULL(fs.f);
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(buf);

    int inr = direntv6_dirlookup(&fs, ROOT_INUMBER, path);
    if (inr < 0) {
        return 0;
    }

    struct filev6 fv6;
    int err = filev6_open(&fs, (uint16_t) inr, &fv6);
    if (err < 0) {
        return 0;
    }

    err = filev6_lseek(&fv6, (int32_t) offset);
    if (err < 0) {
        return 0;
    }

    unsigned char tempBuffer[SECTOR_SIZE];
    memset(tempBuffer, 0, SECTOR_SIZE);
    size_t bytesRead = 0;
    int toAdd = 0;

    // While there is a block to read, we can read it.
    while (bytesRead < size && (toAdd = filev6_readblock(&fv6, tempBuffer)) > 0) {
        memcpy(buf + bytesRead, tempBuffer, (size_t) toAdd);
        bytesRead += (size_t) toAdd;

        memset(tempBuffer, 0, SECTOR_SIZE);
    }
    if (toAdd < 0) {
        return toAdd;
    }

    return (int) bytesRead;
}

static struct fuse_operations available_ops = {
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .read       = fs_read,
};

int main(int argc, char *argv[])
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    int ret = fuse_opt_parse(&args, NULL, NULL, arg_parse);

    if (ret == 0) {
        ret = fuse_main(args.argc, args.argv, &available_ops, NULL);
        (void) umountv6(&fs);
    }

    return ret;
}
