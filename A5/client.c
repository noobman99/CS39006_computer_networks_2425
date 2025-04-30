// =====================================
// Assignment 5 Submission
// Name: Parth Shashin Patil
// Roll number: 22CS30041
// =====================================

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_PORT 8080
#define SERVER_ADDR "127.0.0.1"
#define MAXLINE 100
#define MAX_TASKLEN 20

int main()
{
    struct sockaddr_in servaddr;
    int sockfd, n;
    char buffer[MAXLINE], notask[] = "NO_TASK";
    int op1, op2, result;
    char operator;

    // Create a socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation failed\n");
        exit(0);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_ADDR, &servaddr.sin_addr) <= 0)
    {
        printf("Invalid address/ Address not supported\n");
        exit(0);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("Connection failed\n");
        exit(0);
    }

    while (1)
    {
        memset(buffer, 0, MAXLINE);
        sprintf(buffer, "GET_TASK");

        // Send the request to the server
        if (send(sockfd, buffer, MAXLINE, 0) < 0)
        {
            printf("Send failed\n");
            close(sockfd);
            exit(0);
        }

        memset(buffer, 0, MAXLINE);

        // Receive the response from the server
        if ((n = recv(sockfd, buffer, MAXLINE, MSG_WAITALL)) < 0)
        {
            printf("Receive failed\n");
            close(sockfd);
            exit(0);
        }

        if (n == 0)
        {
            printf("Server disconnected\n");
            close(sockfd);
            break;
        }

        printf("%s\n", buffer);

        int chk = 1, i = 0;
        for (i = 0; i < strlen(notask); i++)
        {
            if (buffer[i] == notask[i])
            {
                continue;
            }
            else
            {
                chk = 0;
                break;
            }
        }

        if (chk)
        {
            printf("No task available\n");
            close(sockfd);
            exit(0);
        }

        sscanf(buffer, "TASK: %d %c %d", &op1, &operator, & op2);

        switch (operator)
        {
        case '+':
            result = op1 + op2;
            break;
        case '-':
            result = op1 - op2;
            break;
        case '*':
            result = op1 * op2;
            break;
        case '/':
            result = op1 / op2;
            break;
        default:
            printf("Invalid operator\n");
            break;
        }

        sleep(1);

        memset(buffer, 0, MAXLINE);
        sprintf(buffer, "RESULT %d", result);

        // Send the result to the server
        if (send(sockfd, buffer, MAXLINE, 0) < 0)
        {
            printf("Send failed\n");
            close(sockfd);
            exit(0);
        }
    }
}