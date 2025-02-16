all: initsocket send recv

send: ksocket.c ksocket.h sender.c
	gcc -o send sender.c ksocket.c -lrt -lpthread

initsocket: initsocket.c ksocket.h
	gcc -o initsocket initsocket.c ksocket.c -lrt -lpthread

recv: ksocket.c ksocket.h reciever.c
	gcc -o recv reciever.c ksocket.c -lrt -lpthread

run_s:
	./send in.txt

run_r:
	./recv output.txt

clean:
	rm -f send initsocket recv