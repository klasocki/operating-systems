CC = gcc -Wall
all: loader trucker spawner

trucker:
	$(CC) trucker.c -o trucker

loader:
	$(CC) loader.c -o loader

spawner:
	$(CC) spawner.c -o spawner

clean:
	rm -rf loader spawner trucker *.o
