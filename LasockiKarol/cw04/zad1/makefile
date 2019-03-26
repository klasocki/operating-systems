all: test clean

main:
	gcc -Wall main.c -o main

main_script:
	gcc -Wall main_script.c -o main_script

test: main main_script
	./main_script
	./main

clean:
	rm -rf *.0
	rm -f main main_script
