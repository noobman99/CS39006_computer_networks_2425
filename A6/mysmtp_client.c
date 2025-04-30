/*
=====================================
Assignment 6 Submission
Name: Parth Shashin Patil
Roll number: 22CS30041
=====================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_BUFFER 4096
#define MAX_LINE 1024

void trim_string(char *str)
{
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n')
    {
        str[len - 1] = '\0';
    }
}

int is_message_end(char *line, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (line[i] == '\0')
        {
            return 1;
        }
    }
    return 0;
}

void receive_response(int sock)
{
    ssize_t bytes_received;
    char line[MAX_LINE];

    while (1)
    {
        bytes_received = recv(sock, line, MAX_LINE - 1, 0);

        if (bytes_received <= 0)
        {
            perror("Receive failed");
            exit(EXIT_FAILURE);
        }

        line[bytes_received] = '\0';

        printf("%s", line);

        // Check for end of message
        if (is_message_end(line, bytes_received))
        {
            break;
        }
    }
}

void handle_data_command(int sock)
{
    char line[MAX_LINE];

    printf("Enter your message (end with a single dot '.'):\n");

    while (1)
    {
        memset(line, 0, MAX_LINE);

        if (fgets(line, MAX_LINE, stdin) == NULL)
        {
            break;
        }

        // Remove trailing newline
        trim_string(line);

        send(sock, line, strlen(line) + 1, 0);

        // Check for end of message
        if (strcmp(line, ".") == 0)
        {
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    int sock = 0;
    struct sockaddr_in serv_addr;
    char command[MAX_LINE] = {0};

    // Check command line arguments
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0)
    {
        perror("Invalid address / Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to My_SMTP server.\n");

    while (1)
    {
        printf("> ");
        if (fgets(command, MAX_LINE, stdin) == NULL)
        {
            break;
        }

        printf("Command: %s", command);

        // Remove trailing newline
        trim_string(command);

        // Check for special handling of DATA command
        if (strcmp(command, "DATA") == 0)
        {
            send(sock, command, strlen(command) + 1, 0);
            char buf[MAX_LINE], tmp[MAX_LINE];
            memset(buf, 0, MAX_LINE);
            while (1)
            {
                memset(tmp, 0, MAX_LINE);
                ssize_t bytes_received = recv(sock, tmp, MAX_LINE - 1, 0);
                if (bytes_received <= 0)
                {
                    perror("Receive failed");
                    exit(EXIT_FAILURE);
                }
                tmp[bytes_received] = '\0';
                strncat(buf, tmp, bytes_received);
                if (is_message_end(tmp, bytes_received))
                {
                    break;
                }
            }
            printf("%s", buf);
            if (strncmp(buf, "200", 3) == 0)
            {
                handle_data_command(sock);
                receive_response(sock);
            }
        }
        else
        {
            send(sock, command, strlen(command) + 1, 0);
            receive_response(sock);

            // If command is QUIT, exit the loop
            if (strncmp(command, "QUIT", 4) == 0)
            {
                break;
            }
        }
    }

    close(sock);
    return 0;
}
