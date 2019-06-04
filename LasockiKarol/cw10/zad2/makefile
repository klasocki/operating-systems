all: server client

server:
	gcc -Wall server.c -o server -lm -lpthread

client:
	gcc -Wall client.c -o client -lm -lpthread

clean:
	rm -rf server client
