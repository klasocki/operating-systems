T1 = ./main test ">" "2019-03-16 21:12:12" nftw
T2 = ./main test ">" "2019-03-16 21:12:12" stat
T3 = ./main test "<" "2019-03-16 21:12:12" stat
	
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
	echo "$(T1)"
	$(T1)
	echo ""
	echo "*************************"
	echo ""
	echo "$(T2)"
	$(T2)
	echo "$(T3)"
	$(T3)
	rm -r test

clean:
	rm -rf *.o
	rm -f main
