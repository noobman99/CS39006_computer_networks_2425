/*
=====================================
Assignment 2 Submission
Name: Parth Shashin Patil
Roll number: 22CS30041
Link of the pcap file: https://drive.google.com/file/d/1D-C1VmfawQ1m-aEOM3uV6uMR-sVzY3xC/view?usp=sharing
=====================================
*/

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>

#define PORT 5000
#define ADDR "127.0.0.1"

int main()
{
    char buffer[100], filename[100], outputfilename[150];
    char *error = "NOTFOUND";
    char *fileend = "FINISH";
    char *word = "word";
    int sockfd, n;
    struct sockaddr_in servaddr;

    // clear servaddr
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = inet_addr(ADDR);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET;

    // create datagram socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // get the filename
    printf("Enter filename to transfer:\n");
    scanf("%s", filename);

    // request filename from server
    sendto(sockfd, filename, strlen(filename) + 1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
    // waiting for response
    n = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);

    if (n == -1)
    {
        printf("Uncaught error during message transfer\n");
    }

    // check for error
    int chk = 1;
    for (int i = 0; i < 8; i++)
    {
        chk = chk && (error[i] == buffer[i]);
    }
    if (chk)
    {
        printf("File not found.\nExiting the program.\n");
        close(sockfd);
        return 0;
    }

    // fetch message.
    sprintf(outputfilename, "out_%s", filename);
    FILE *fp = fopen(outputfilename, "w");
    fprintf(fp, "%s\n", buffer);

    while (strcmp(fileend, buffer))
    {
        // request for word
        sendto(sockfd, word, strlen(word) + 1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
        // waiting for response
        recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);

        // print into file
        fprintf(fp, "%s\n", buffer);
    }

    fclose(fp);

    printf("File transferred!\n");

    // close the descriptor
    close(sockfd);
}
