# Valgrind: valgrind --leak-check=yes --track-origins=yes --leak-check=full --show-leak-kinds=all exec args

CFLAGS += -std=c99 -pedantic -Wall -Wextra -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual -Wcast-align -Wwrite-strings -Wconversion -Wunreachable-code
LDFLAGS += -lcrypto

all: cleanBefore tests shell fs cleanAfter

tests: test-inodes test-file test-dirent test-bitmap

cleanAll: cleanBefore cleanAfter

fs.o: fs.c error.h direntv6.h unixv6fs.h filev6.h mount.h bmblock.h sector.h inode.h
	$(COMPILE.c) -D_DEFAULT_SOURCE $$(pkg-config fuse --cflags) -o $@ -c $<

test-inodes: test-inodes.o error.o test-core.o inode.o mount.o sector.o bmblock.o test-core.o
	gcc $(CFLAGS) -g -o test-inodes $^ -ggdb

test-file: test-file.o filev6.o mount.o bmblock.o error.o inode.o sha.o sector.o test-core.o
	gcc $(CFLAGS) -g -o test-file $^ $(LDFLAGS) -ggdb

test-dirent: test-dirent.o mount.o bmblock.o direntv6.o filev6.o test-core.o sector.o error.o inode.o
	gcc $(CFLAGS) -g -o test-dirent $^ -ggdb

test-bitmap: test-bitmap.o bmblock.o 
	gcc $(CFLAGS) -g -o test-bitmap $^ -ggdb

shell: shell.o mount.o bmblock.o inode.o filev6.o direntv6.o error.o sector.o sha.o
	gcc $(CFLAGS) -g -o shell $^ $(LDFLAGS) -ggdb

fs: fs.o mount.o error.o direntv6.o filev6.o inode.o sector.o bmblock.o
	$(LINK.c) -g -o $@ $^ $(LDLIBS) $$(pkg-config fuse --libs) ${LIBS}

cleanBefore:
	rm -rf fs shell test-bitmap test-dirent test-file test-inodes

cleanAfter:
	rm -rf *.o
	rm -rf *.gch

