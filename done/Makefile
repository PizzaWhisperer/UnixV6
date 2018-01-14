# Valgrind: valgrind --leak-check=yes --track-origins=yes --leak-check=full --show-leak-kinds=all exec args

CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wunreachable-code

LDFLAGS += -lcrypto

GGDB += -ggdb

all: cleanBefore replaceDisksWithFreshOnes tests shell fs cleanAfter

tests: test-inodes test-file test-dirent test-bitmap test-bmmount test-create

cleanAll: cleanBefore replaceDisksWithFreshOnes cleanAfter

fs.o: fs.c error.h direntv6.h unixv6fs.h filev6.h mount.h bmblock.h sector.h inode.h
	$(COMPILE.c) -D_DEFAULT_SOURCE $$(pkg-config fuse --cflags) -o $@ -c $<

test-inodes: test-inodes.o error.o test-core.o inode.o mount.o sector.o bmblock.o test-core.o
	gcc $(CFLAGS) -g -o test-inodes $^ $(GGDB)

test-file: test-file.o filev6.o mount.o bmblock.o error.o inode.o sha.o sector.o test-core.o
	gcc $(CFLAGS) -g -o test-file $^ $(LDFLAGS) $(GGDB)

test-dirent: test-dirent.o mount.o bmblock.o direntv6.o filev6.o test-core.o sector.o error.o inode.o
	gcc $(CFLAGS) -g -o test-dirent $^ $(GGDB)

test-bitmap: test-bitmap.o bmblock.o
	gcc $(CFLAGS) -g -o test-bitmap $^ $(GGDB)

shell: shell.o mount.o bmblock.o inode.o filev6.o direntv6.o error.o sector.o sha.o
	gcc $(CFLAGS) -g -o shell $^ $(LDFLAGS) $(GGDB)

fs: fs.o mount.o error.o direntv6.o filev6.o inode.o sector.o bmblock.o
	$(LINK.c) -g -o $@ $^ $(LDLIBS) $$(pkg-config fuse --libs) ${LIBS}

test-bmmount: test-bmmount.o bmblock.o test-core.o mount.o inode.o error.o sector.o
	gcc $(CFLAGS) -g -o test-bmmount $^ $(GGDB)

test-create: test-create.o bmblock.o test-core.o inode.o error.o sector.o mount.o filev6.o direntv6.o
	gcc $(CFLAGS) -g -o test-create $^ $(GGDB)

replaceDisksWithFreshOnes:
	@printf "\n===================REFRESH_DISKS===================\n\n"
	rm -v -rf disks/*.uv6
	cp -v disks/BACKUP/*.uv6 disks/
	@printf "\n"

cleanBefore:
	@printf "\n===================CLEAN_BEFORE===================\n\n"
	rm -v -rf fs shell test-bitmap test-dirent test-file test-inodes test-bmmount test-create
	@printf "\n"

cleanAfter:
	@printf "\n===================CLEAN_AFTER===================\n\n"
	rm -v -rf *.o
	rm -v -rf *~
	rm -v -rf disks/*~
	rm -v -rf *.gch
	@printf "\n"
