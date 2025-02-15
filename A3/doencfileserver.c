/*
=====================================
Assignment 3 Submission
Name: Parth Shashin Patil
Roll number: 22CS30041
Link of the pcap file: https://drive.google.com/drive/folders/1nS5f326pF1DT1AL9OPK9o1cdbaThz9Jw?usp=sharing
=====================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_REQUESTS 10
#define BUF_SIZE 100

// clear the buffer
void clear_buf(char buf[], int n)
{
    for (int i = 0; i < n; i++)
    {
        buf[i] = '\0';
    }
}

// function to encrypt
void encrypt(char input[], char output[], char key[])
{
    FILE *fin, *fout;
    char c, d;
    fin = fopen(input, "r");
    fout = fopen(output, "w");
    while ((c = fgetc(fin)) > 0)
    {
        if (c >= 'a' && c <= 'z')
        {
            d = key[c - 'a'];
            d = d - 'A' + 'a';
        }
        else if (c >= 'A' && c <= 'Z')
        {
            d = key[c - 'A'];
        }
        else
        {
            d = c;
        }
        fputc(d, fout);
    }
    fclose(fin);
    fclose(fout);
}

// Check if the buffer contains \0
int notends(char buf[], int n)
{
    for (int i = 0; i < n; i++)
    {
        if (buf[i] == '\0')
        {
            return 0;
        }
    }
    return 1;
}

int main()
{
    int sockfd, newsockfd; /* Socket descriptors */
    int clilen;
    struct sockaddr_in cli_addr, serv_addr;
    int i, kidx, cread;
    char c;
    char buf[BUF_SIZE];
    char key[26], filename[30], modified_filename[34];
    int num_connections = 0;
    FILE *fp;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Cannot create socket\n");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(20000);

    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
    {
        perror("Unable to bind local address\n");
        exit(0);
    }

    listen(sockfd, 5);

    while (1)
    {
        printf("Waiting for connection\n");

        if (num_connections >= MAX_REQUESTS)
        {
            // if number of connections > max request, wait for a connection to close
            wait(NULL);
            num_connections--;
        }

        // wait for new connection
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);

        if (newsockfd < 0)
        {
            perror("Accept error\n");
            exit(0);
        }

        if (fork() != 0)
        {
            close(newsockfd);
            num_connections++;
            continue;
        }

        close(sockfd);
        // new connection

        printf("Recieved a connection from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        while (1)
        {
            // Initialize the variables
            sprintf(filename, "%s.%d.txt", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            sprintf(modified_filename, "%s.enc", filename);

            clear_buf(buf, BUF_SIZE);
            clear_buf(key, 26);

            fp = fopen(filename, "w");
            kidx = 0;

            printf("listener(%s:%d): Waiting for input from client.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            while ((cread = recv(newsockfd, buf, BUF_SIZE, 0)) > 0 && notends(buf, cread))
            {
                for (int i = 0; i < cread; i++)
                {
                    if (kidx < 26)
                    {
                        // take input of the key for encryption
                        key[kidx++] = buf[i];
                    }
                    else
                    {
                        fputc(buf[i], fp);
                    }
                }
                clear_buf(buf, BUF_SIZE);
            }
            for (int i = 0; i < cread - 1; i++)
            {
                fputc(buf[i], fp);
            }
            clear_buf(buf, BUF_SIZE);
            fclose(fp);

            printf("listener(%s:%d): Encrypting the file.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            // encrypt the file
            encrypt(filename, modified_filename, key);

            // send the file
            fp = fopen(modified_filename, "r");
            i = 0;
            printf("listener(%s:%d): Sending encrypted file to client.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            while ((c = fgetc(fp)) > 0)
            {
                buf[i++] = c;

                if (i == BUF_SIZE)
                {
                    send(newsockfd, buf, BUF_SIZE, 0);
                    i = 0;
                    clear_buf(buf, BUF_SIZE);
                }
            }
            send(newsockfd, buf, strlen(buf) + 1, 0);
            clear_buf(buf, BUF_SIZE);
            fclose(fp);

            // check if there are more files
            cread = recv(newsockfd, buf, BUF_SIZE, 0);
            if (cread <= 0 || buf[0] == 'N')
            {
                printf("listener(%s:%d): Terminating connection.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                break;
            }
            printf("listener(%s:%d): Waiting for new input from client.\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
        }
        // kill the child
        close(newsockfd);
        exit(0);
    }

    close(sockfd);

    return 0;
}
