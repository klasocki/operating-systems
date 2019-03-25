all: test clean

test: tester monitor
	touch file
	touch plik
	echo "file 1\nplik 4" >> test.txt
	./tester file 1 2 25 13& ./tester plik 2 7 25 9& ./monitor test.txt 25 mem 50 10
	./tester file 1 2 15 111300& ./tester plik 2 7 15 944000& ./monitor test.txt 15 mem 5 1

monitor:
	gcc -Wall monitor.c -o monitor

tester:
	gcc -Wall tester.c -o tester

clean:
	rm -rf *.o
	rm -f monitor tester test.txt file* plik*

