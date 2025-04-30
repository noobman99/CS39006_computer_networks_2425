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

#define INT_MIN -1e9
#define PORT 8000
#define MAXSIZE 1000
#define MAXCLIENTS 5


int max(int x, int y) {
    return (x>y) ? x:y;
}

int main() {
    struct sockaddr_in serveraddr, cliaddr;
    int client_entries[MAXSIZE];
    int clientsockfd[MAXSIZE];
    int blacklist[MAXSIZE];

    memset(clientsockfd, 0, MAXSIZE);
    memset(client_entries, 0, MAXSIZE);
    memset(blacklist, 0, MAXSIZE);
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(PORT);

    int servsockfd = socket(AF_INET, SOCK_STREAM, 0);
    int newsockfd, numclient = 0;
    if (servsockfd < 0) {
        printf("Cannot allocate socket\n");
        exit(0);
    }
    if (bind(servsockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        printf("Cannot bind to socket\n");
        exit(0);
    }

    fd_set readfds;
    int numfds;

    int roundnum = 1, clentry, maxclentry = INT_MIN, maxclentryfd = -1, entrycnt = 0;
    char buf[MAXSIZE];

    listen(servsockfd, 10);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(servsockfd, &readfds);
        numfds = max(0, servsockfd);

        for (int i = 0; i < numclient; i++) {
            if(blacklist[i]) continue;

            FD_SET(clientsockfd[i], &readfds);
            numfds = max(numfds, clientsockfd[i]);
        }

        if (select(numfds+1, &readfds, NULL, NULL, NULL) < 1) {
            printf("Printf select error\n");
            exit(0);
        }

        if (FD_ISSET(servsockfd, &readfds)) {
            int clilen = sizeof(cliaddr);
            newsockfd = accept(servsockfd, (struct sockaddr *) &cliaddr, &clilen);
            if (newsockfd < 0) {
                printf("New client could not be accepted\n");
            } else {
                numclient++;
                clientsockfd[numclient - 1] = newsockfd;
                printf("Server: Received a new connection from client %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                sprintf(buf, "Send your number for Round %d:", roundnum);
                send(newsockfd, buf, strlen(buf), 0);
            }
        }

        int nread;

        for (int i = 0; i < numclient; i++) {

            if (blacklist[i]) {
                if (client_entries[i] == 0) {
                    client_entries[i] = 1;
                    entrycnt++;
                }
                continue;
            }
        
            memset(buf, 0, MAXSIZE);
            if (FD_ISSET(clientsockfd[i], &readfds)) {
                nread = recv(clientsockfd[i], buf, MAXSIZE, 0);

                if (nread == 0) {
                    blacklist[i] = 1;
                    continue;
                }

                int clilen = sizeof(cliaddr);
                if (getpeername(clientsockfd[i], (struct sockaddr *) &cliaddr, &clilen) < 0) {
                    printf("Error in getting peer name\n");
                    continue;
                }

                if (numclient < 2) {
                    printf("Server: Insufficient clients, \"%s\" from client %s:%d dropped\n", buf, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                    continue;
                }

                if (sscanf(buf, "%d", &clentry) < 0) {
                    printf("Client %s:%d sent non integer to server\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                    continue;
                }

                if (client_entries[i]) {
                    memset(buf, 0, MAXSIZE);
                    sprintf(buf, "Duplicate messages for Round %d are not allowed. Please wait for results of Round %d and Call for the number for Round %d", roundnum, roundnum, roundnum + 1);
                    send(clientsockfd[i], buf, strlen(buf), 0);
                    continue;
                }


                client_entries[i] = 1;
                entrycnt++;

                printf("Recieved %d from client %s:%d\n", clentry, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));


                if (clentry > maxclentry) {
                    maxclentry = clentry;
                    maxclentryfd = clientsockfd[i];
                }
            }
        }

        if (entrycnt == numclient && numclient >= 2) {

            if (maxclentryfd == -1) {
                printf("Some blasphemy\n");
                exit(0);
            }

            entrycnt = 0;
            memset(buf, 0, MAXSIZE);
            int clilen = sizeof(cliaddr);
            getpeername(maxclentryfd, (struct sockaddr *) &cliaddr, &clilen);
            sprintf(buf, "Maximum Number Received in Round %d is: %d. The number has been received from the client %s:%d\n", roundnum, maxclentry, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
            for (int i = 0; i < numclient; i++) {
                if (blacklist[i]) {
                    continue;
                }
                send(clientsockfd[i], buf, strlen(buf), 0);
            }
            memset(client_entries, 0, MAXSIZE);
            maxclentry = INT_MIN;
            maxclentryfd = -1;
            roundnum++;

            // ADDED SLEEP TO FORCE SYNCHONIZATION BETWEEN MESSAGES
            sleep(1);

            memset(buf, 0, MAXSIZE);
            sprintf(buf, "Enter number for Round %d:", roundnum);
            for (int i = 0; i < numclient; i++) {
                if (blacklist[i]) {
                    continue;
                }
                send(clientsockfd[i], buf, strlen(buf), 0);
            }
        }
    }
}