CC = gcc -Wall
all: loader trucker spawner

trucker:
	$(CC) trucker.c -o trucker -l pthread -l rt

loader:
	$(CC) loader.c -o loader -l pthread -l rt

spawner:
	$(CC) spawner.c -o spawner

clean:
	rm -rf loader spawner trucker *.o
