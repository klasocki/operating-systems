all: master slave test clean

master:
	gcc -Wall master.c -o master

slave:
	gcc -Wall slave.c -o slave

test:
	./master pipe & ./slave pipe 5

clean:
	rm -f master slave pipe
