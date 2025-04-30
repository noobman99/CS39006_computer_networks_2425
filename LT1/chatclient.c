// NAME - Parth Shashin Patil
// ROLL - 22CS30041
// Question Set - B

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8000
#define MAXSIZE 1000

int max(int x, int y) {
    return (x>y) ? x:y;
}

int main() {
    struct sockaddr_in serveraddr, selfaddr;

    memset(&serveraddr, 0, sizeof(serveraddr));
    inet_aton("127.0.0.1", &serveraddr.sin_addr);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(PORT);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int selflen;

    if (sockfd < 0) {
        printf("Cannot allocate socket\n");
        exit(0);
    }

    if (connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        printf("Could not connect to server\n");
        exit(0);
    }

    fd_set readfds;
    int numfds, nread, clentry;
    char buf[MAXSIZE];

    selflen = sizeof(selfaddr);
    if (getsockname(sockfd, (struct sockaddr *) &selfaddr, &selflen) < 0) {
        printf("Could not get own address\n");
        exit(0);
    }

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        numfds = max(0, STDIN_FILENO);
        FD_SET(sockfd, &readfds);
        numfds = max(sockfd, numfds);

        if (select(numfds+1, &readfds, NULL, NULL, NULL) < 1) {
            printf("Printf select error\n");
            exit(0);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buf, 0, MAXSIZE);
            nread = read(STDIN_FILENO, buf, MAXSIZE);
            if (sscanf(buf, "%d", &clentry)) {
                memset(buf, 0, MAXSIZE);
                sprintf(buf, "%d", clentry);
                send(sockfd, buf, strlen(buf) + 1, 0);
                printf("Client %s:%d Number %d to server\n", inet_ntoa(selfaddr.sin_addr), ntohs(selfaddr.sin_port), clentry);
            } else {
                printf("Enter integers only!\n");
            }
        }

        if (FD_ISSET(sockfd, &readfds)) {
            memset(buf, 0, MAXSIZE);
            nread = recv(sockfd, buf, MAXSIZE, 0);
            if (nread == 0) {
                printf("Socket closed!");
                close(sockfd);
                exit(0);
            }
            buf[nread] = '\0';
            printf("Client: Received the following message text from the server: %s\n", buf);
        }
    }
}