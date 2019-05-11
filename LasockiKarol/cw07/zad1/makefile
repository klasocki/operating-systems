CC = gcc -Wall
all: loader trucker spawner test clean

compile-all: loader trucker spawner

trucker:
	$(CC) trucker.c -o trucker

loader:
	$(CC) loader.c -o loader

spawner:
	$(CC) spawner.c -o spawner

test:
	./trucker 17 10 50 & ./spawner 7 9 50

clean:
	rm -rf loader spawner trucker *.o
