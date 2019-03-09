CC = gcc -Wall -std=c99

all: static shared dynamic test clean

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
	$(CC) main.c libfind_lib.so -L. -o main_shared

dynamic:
	$(CC) -fPIC -c find_lib.c -o libfind_lib.o
	$(CC) -fPIC -shared libfind_lib.o -o libfind_lib.so
	$(CC) main.c -o main_dynamic -ldl -D DLL

clean:
	rm -rf *.o
	rm -rf *.so
	rm -rf *.a

test: test_static test_dynamic

test_static:
	./main_static 10 search_directory / bin tmp.txt search_directory /etc/ passwd tmp.txt search_directory ~ file tmp.txt remove_block_index 0 remove_block_index 1 remove_block_index 2 remove_block_index 3
	./main_static 10000 add_and_delete 50 100 tmp.txt

test_dynamic:
	./main_dynamic 10 search_directory / bin tmp2.txt search_directory /etc/ passwd tmp2.txt search_directory ~ file tmp2.txt remove_block_index 0 remove_block_index 1 remove_block_index 2 remove_block_index 3
	./main_dynamic 10000 add_and_delete 50 100 tmp2.txt

report: static dynamic
	echo "Static library: " >> results3a.txt
	make test_static 1>> results3a.txt
	echo "Dynamic library: " >> results3a.txt
	make test_dynamic 1>> results3a.txt

clean_all: clean
	rm -rf main_static main_shared main_dynamic *tmp.txt tmp*.txt