/*
=====================================
Assignment 2 Submission
Name: Parth Shashin Patil
Roll number: 22CS30041
Link of the pcap file: https://drive.google.com/file/d/1D-C1VmfawQ1m-aEOM3uV6uMR-sVzY3xC/view?usp=sharing
=====================================
*/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000

void clearbuf(char buf[], int n)
{
    for (int i = 0; i < n; i++)
    {
        buf[i] = '\0';
    }
}

int main()
{
    char buffer[100], message[100];
    char *filename;
    char *finish = "FINISH";
    int serverfd, n;
    socklen_t len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    serverfd = socket(AF_INET, SOCK_DGRAM, 0);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    // bind server address to socket descriptor
    bind(serverfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    printf("Server is listening\n");

    // receive the datagram
    len = sizeof(cliaddr);
    n = recvfrom(serverfd, buffer, sizeof(buffer),
                 0, (struct sockaddr *)&cliaddr, &len); // receive message from server
    buffer[n] = '\0';
    filename = strdup(buffer);

    FILE *fp = fopen(filename, "r");

    clearbuf(message, 100);

    if (fp == NULL)
    {
        sprintf(message, "NOTFOUND %s", filename);
        sendto(serverfd, message, strlen(message) + 1, 0,
               (struct sockaddr *)&cliaddr, sizeof(cliaddr));
        printf("File not found.\nExiting the program.\n");
        close(serverfd);
        return 0;
    }

    printf("Sending %s to client\n", filename);

    // clear buffer to avoid garbage values
    fscanf(fp, "%s", message);
    sendto(serverfd, message, strlen(message) + 1, 0,
           (struct sockaddr *)&cliaddr, sizeof(cliaddr));

    while (strcmp(message, finish))
    {
        // recieve message from client
        n = recvfrom(serverfd, buffer, sizeof(buffer),
                     0, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';

        // clear buffer to avoid garbage values
        clearbuf(message, 100);
        fscanf(fp, "%s", message);
        sendto(serverfd, message, strlen(message) + 1, 0,
               (struct sockaddr *)&cliaddr, sizeof(cliaddr));
    }

    printf("File sent to client\n");

    close(serverfd);

    return 0;
}
