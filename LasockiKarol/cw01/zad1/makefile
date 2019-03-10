CC = gcc -Wall -std=c99 -g

all: static shared

static:
# build sources
	$(CC) -c find_lib.c
 # build static lib
	ar rcs find_lib.a find_lib.o

shared:
	$(CC) -fPIC -c find_lib.c -o libfind_lib.o
	$(CC) -fPIC -shared libfind_lib.o -o libfind_lib.so

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf *.a

