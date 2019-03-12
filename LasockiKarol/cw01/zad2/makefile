CC = gcc -Wall -std=c99

all: static shared test clean

static:
# build sources
	$(CC) -c find_lib.c
# build static lib
	ar rcs find_lib.a find_lib.o
# link
	$(CC) -o main_static main.c find_lib.a -I.

shared:
	$(CC) -fPIC -c find_lib.c -o libfind_lib.o
	$(CC) -fPIC -shared libfind_lib.o -o libfind_lib.so
	$(CC) main.c libfind_lib.o -L. -o main_shared

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf *.a

test: test_static test_shared

test_static:
	./main_static 10 search_directory / bin tmp.txt search_directory /etc/ passwd tmp.txt search_directory ~ file tmp.txt remove_block_index 0 remove_block_index 1 remove_block_index 2 remove_block_index 3
	./main_static 10000 add_and_delete 50 100 tmp.txt

test_shared:
	./main_shared 10 search_directory / bin tmp3.txt search_directory /etc/ passwd tmp3.txt search_directory ~ file tmp3.txt remove_block_index 0 remove_block_index 1 remove_block_index 2 remove_block_index 3
	./main_shared 10000 add_and_delete 50 100 tmp3.txt

report: static
	make test_static 1> raport2.txt
	make clean_all

clean_all: clean
	rm -rf main_static main_shared *tmp.txt tmp*.txt
