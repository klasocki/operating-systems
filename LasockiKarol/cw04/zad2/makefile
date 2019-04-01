all: test clean

test: monitor
	touch file
	touch plik
	echo "file 1\nplik 4" >> test.txt
	./monitor test.txt

monitor:
	gcc -Wall monitor.c -o monitor

clean:
	rm -rf *.o
	rm -f monitor test.txt file* plik*

