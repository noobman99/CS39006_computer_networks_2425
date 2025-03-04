all: initsocket send recv

initsocket: initsocket.c ksocket.h libsocket.a
	gcc -o initsocket initsocket.c ksocket.c -L. -lsocket -lrt -lpthread

send: ksocket.h user1.c libsocket.a
	gcc -o send user1.c ksocket.c -L. -lsocket -lrt -lpthread

recv: ksocket.h user2.c libsocket.a
	gcc -o recv user2.c ksocket.c -L. -lsocket -lrt -lpthread

libsocket.a: ksocket.c
	gcc -c ksocket.c -lrt -lpthread
	ar rcs libsocket.a ksocket.o

run_daemon: initsocket
	./initsocket

run_s: send
	./send in.txt

run_r: recv
	./recv output.txt

clean:
	rm -f send initsocket recv libsocket.a ksocket.o output.txt