all: server client test clean

server:
	gcc -Wall server.c -o server

client:
	gcc -Wall client.c -o client

test:
	./server & ./client & ./client & ./client

clean:
	rm -rf server client
