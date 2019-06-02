all: test clean
main:
	gcc -Wall -o main main.c -lm -lpthread

clean:
	rm -f main

test: main
	./main 19 3 5 2