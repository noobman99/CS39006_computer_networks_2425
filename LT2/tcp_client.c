#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9090

int main() {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_family = AF_INET;
    inet_aton(SERVER_IP, (struct in_addr *) &server_addr.sin_addr.s_addr);

    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("connect()");
        close(sockfd);
        return 1;
    }

    char buff[100];
    char c;

    while(1) {
        int i = 0, br = 0, brt = 0;
        memset(buff, 0, 100);
        printf("Enter your message: \n");
        scanf("%c", &c);
        while (c != '\n') {
            buff[i] = c;
            i++;
            scanf("%c", &c);
        }

        buff[i] = '\0';
        
        send(sockfd, buff, i, 0);
        printf("Server replied with : ");
        while (brt < i) {
            memset(buff, 0, 100);
            br = recv(sockfd, buff, 100, 0);
            buff[br] = '\0';
            printf("%s", buff);
            brt += br;
        }
        printf("\n");
    }
}