all: main test clean

main:
	gcc -Wall main.c -o main

test: main
	echo "echo hello | grep he" > commands.txt
	echo "ps aux | grep root | head -n 3" >> commands.txt
	./main commands.txt

clean:
	rm -f main commands.txt
