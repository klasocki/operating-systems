all: test clean

test: tester monitor
	touch file
	touch plik
	echo "file 1\nplik 4" >> test.txt
	./tester file 1 2 25 11& ./tester plik 2 7 25 9& ./monitor test.txt 25 mem

monitor:
	gcc -Wall monitor.c -o monitor

tester:
	gcc -Wall tester.c -o tester

clean:
	rm -rf *.o
	rm -f monitor tester test.txt file* plik*

