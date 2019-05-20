main:
	gcc -Wall -o main main.c -lm -lpthread

clean: 
	rm -f main

test: main
	./test.sh > Times.txt
