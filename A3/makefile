run_client: retrieveencfileclient.c
	gcc -o client retrieveencfileclient.c
	./client

run_server: doencfileserver.c
	gcc -o server doencfileserver.c
	./server

clean:
	rm -f server client 127.0.0.1.* *.enc