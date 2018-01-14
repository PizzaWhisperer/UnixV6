CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wunreachable-code
#CFLAGS += -std=c99

all: test-inodes test-file test-dirent shell

test-core.o: test-core.c mount.h unixv6fs.h bmblock.h error.h

error.o: error.c

test-inodes.o: test-inodes.c inode.h unixv6fs.h mount.h bmblock.h error.h

inode.o: inode.c inode.h unixv6fs.h mount.h bmblock.h sector.h error.h

mount.o: mount.c mount.h unixv6fs.h bmblock.h error.h sector.h

sector.o: sector.c error.h sector.h unixv6fs.h

filev6.o: filev6.c filev6.h unixv6fs.h mount.h bmblock.h inode.h error.h sector.h

sha.o: sha.h filev6.h sha.c mount.h unixv6fs.h bmblock.h sector.h inode.h filev6.h

direntv6.o: direntv6.c error.h direntv6.h unixv6fs.h filev6.h mount.h bmblock.h

shell.o: shell.c sha.h filev6.h inode.h mount.h unixv6fs.h sector.h direntv6.h error.h

fs.o: fs.c
	$(COMPILE.c) -D_DEFAULT_SOURCE $$(pkg-config fuse --cflags) -o $@ -c $<

#correcteur : les directives de compilation comme -ggdb et l'ajout de librairie comme -lcrypto, puisque utilisées au moins deux fois auraient du être mis dans des flags (-1)
test-inodes: test-inodes.o error.o test-core.o inode.o mount.o sector.o
	gcc $(CFLAGS) -o test-inodes test-inodes.o error.o test-core.o inode.o mount.o sector.o -ggdb

test-file: test-file.o error.o test-core.o inode.o mount.o sector.o sha.o filev6.o
	gcc $(CFLAGS) -o test-file test-file.o error.o test-core.o inode.o mount.o sector.o sha.o filev6.o -lcrypto -ggdb

test-dirent: test-dirent.o direntv6.o error.o test-core.o mount.o filev6.o sector.o inode.o
	gcc $(CFLAGS) -o test-dirent test-dirent.o direntv6.o error.o test-core.o mount.o filev6.o sector.o inode.o -ggdb

shell: shell.o inode.o mount.o sector.o sha.o filev6.o direntv6.o error.o
	gcc $(CFLAGS) -o shell shell.o inode.o mount.o sector.o sha.o filev6.o direntv6.o error.o -lcrypto -ggdb

fs: fs.o
	$(LINK.c) -o $@ $^ $(LDLIBS) $$(pkg-config fuse --libs)

