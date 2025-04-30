all: cldp_server cldp_client

cldp_server: cldp_server.c
	gcc -o cldp_server cldp_server.c -lpthread

cldp_client: cldp_client.c
	gcc -o cldp_client cldp_client.c -lpthread

clean:
	rm -f cldp_server cldp_client
