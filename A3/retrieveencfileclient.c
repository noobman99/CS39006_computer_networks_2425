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
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 100

// validating the key input
void input_key(char key[])
{
    while (1)
    {
        printf("Enter key > ");
        scanf("%s", key);
        if (strlen(key) != 26)
        {
            printf("Length of Key must be 26 characters");
        }
        else
        {
            break;
        }
    }
}

// clear the buffer
void clear_buf(char buf[], int n)
{
    for (int i = 0; i < n; i++)
    {
        buf[i] = '\0';
    }
}

// check if buf contains \0
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
    int sockfd;
    struct sockaddr_in serv_addr;
    int i, cread;
    char buf[BUF_SIZE];
    char key[27];
    char filename[100], modified_filename[104];
    FILE *fp;
    char c, chk[10];

    /* Opening a socket is exactly similar to the server process */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Unable to create socket\n");
        exit(0);
    }

    serv_addr.sin_family = AF_INET;
    inet_aton("127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(20000);

    /* With the information specified in serv_addr, the connect()
       system call establishes a connection with the server process.
    */
    if ((connect(sockfd, (struct sockaddr *)&serv_addr,
                 sizeof(serv_addr))) < 0)
    {
        perror("Unable to connect to server\n");
        exit(0);
    }

    while (1)
    {
        printf("Enter filename > ");
        scanf("%s", filename);

        fp = fopen(filename, "r");
        if (fp == NULL)
        {
            printf("NOTFOUND %s\n", filename);
            continue;
        }

        input_key(key);

        send(sockfd, key, 26, 0);
        printf("Sending file %s\n", filename);
        // send the file
        i = 0;
        clear_buf(buf, BUF_SIZE);
        while ((c = fgetc(fp)) > 0)
        {
            buf[i++] = c;

            if (i == BUF_SIZE)
            {
                send(sockfd, buf, BUF_SIZE, 0);
                i = 0;
                clear_buf(buf, BUF_SIZE);
            }
        }
        send(sockfd, buf, strlen(buf) + 1, 0);
        clear_buf(buf, BUF_SIZE);
        fclose(fp);

        // recieve the file
        sprintf(modified_filename, "%s.enc", filename);

        fp = fopen(modified_filename, "w");
        printf("Waiting to recieve the encrypted file.\n");
        while ((cread = recv(sockfd, buf, BUF_SIZE, 0)) > 0 && notends(buf, cread))
        {
            for (i = 0; i < cread; i++)
            {
                fputc(buf[i], fp);
            }
            clear_buf(buf, BUF_SIZE);
        }
        for (i = 0; i < cread - 1; i++)
        {
            fputc(buf[i], fp);
        }
        clear_buf(buf, BUF_SIZE);
        printf("Encrypted file written in %s.\n", modified_filename);

        fclose(fp);

        // check if wanting to continue
        printf("Do you want to continue? (yes/no) > ");
        scanf("%s", chk);

        if (strcmp(chk, "no") == 0)
        {
            buf[0] = 'N';
            send(sockfd, buf, 1, 0);
            break;
        }
        buf[0] = 'Y';
        send(sockfd, buf, 1, 0);

        clear_buf(buf, BUF_SIZE);
    }
    // close the connection
    close(sockfd);
    return 0;
}
