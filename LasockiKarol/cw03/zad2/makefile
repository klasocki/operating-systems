T = ./main test
	
all: main test clean

main:
	gcc -Wall main.c -o main

directory:
	mkdir test
	mkdir test/kat1
	mkdir test/kat2
	mkdir test/kat1/subkat
	touch test/file
	touch test/kat1/file
	touch test/kat1/subkat/file
	ln -s /etc/passwd test/passwd-link
	ln -s ~ test/kat2/home-link

test: main directory
	$(T)

clean:
	rm -rf *.o
	rm -f main
	rm -r test
