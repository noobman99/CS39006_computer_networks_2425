all: tcp_client tcp_echo_server

tcp_client: tcp_client.c
	gcc -o tcp_client -Wall tcp_client.c

tcp_echo_server: tcp_echo_server.c
	gcc -o tcp_echo_server -Wall tcp_echo_server.c

clean:
	rm -f tcp_client tcp_echo_server