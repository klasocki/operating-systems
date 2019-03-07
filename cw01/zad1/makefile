CC = gcc -Wall -std=c99 -g

all: static shared

static:
# build sources
	$(CC) -c find_lib.c
 # build static lib
	ar rcs find_lib.a find_lib.o


shared:
	$(CC) -fPIC -c find_lib.c
	$(CC) -fPIC -shared -o find_lib.so find_lib.o

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf *.a

