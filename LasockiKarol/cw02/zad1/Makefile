all: main test clean

main:
	gcc -Wall -std=c99 main.c -o main

test:
	./main generate tmp.txt 10 2 sort tmp.txt 10 2 sys copy tmp.txt tmp_cpy.txt 10 2 lib

report: main 
	chmod +x test.sh
	./test.sh > wyniki.txt

clean:
	rm -rf *.o
	rm -f main

clean_all: clean
	rm *tmp*.txt
