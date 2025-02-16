#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ksocket.h"

extern int errno;

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define CLIENT_PORT 8081
#define MESSAGE_SIZE 512

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <output_filename>\n", argv[0]);
        exit(1);
    }

    // Open the output file
    FILE *file = fopen(argv[1], "wb");
    if (!file)
    {
        perror("Error opening output file");
        exit(1);
    }

    // Create a socket
    ktp_sockid sock = k_socket(AF_INET, SOCK_KTP, 0);
    if (sock < 0)
    {
        perror("Error creating socket");
        fclose(file);
        exit(1);
    }

    printf("Got socket %d\n", sock);

    // Set up the server address (this program)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Set up the client address
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    inet_pton(AF_INET, SERVER_IP, &client_addr.sin_addr);

    // Bind the socket
    if (k_bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr),
               (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Error binding socket");
        k_close(sock);
        fclose(file);
        exit(1);
    }

    printf("Socket bound successfully. Waiting for data...\n");

    // Buffer for receiving data
    char buffer[MESSAGE_SIZE + 1];
    socklen_t addr_len = sizeof(client_addr);
    int packets_received = 0;

    while (1)
    {
        // Clear the buffer
        memset(buffer, 0, MESSAGE_SIZE + 1);

        // Receive data
        ssize_t bytes_received = k_recvfrom(sock, buffer, MESSAGE_SIZE, 0,
                                            (struct sockaddr *)&client_addr, &addr_len);

        if (bytes_received < 0)
        {
            printf("Error receiving data\n");

            printf("Err number is : %d\n", errno);

            if (errno != ENOMESSAGE)
            {
                k_close(sock);
                fclose(file);
                return 0;
            }

            sleep(1); // 100 milliseconds sleep
            continue; // Try again instead of breaking
        }

        packets_received++;

        printf("For %d: first byte is %d\n", packets_received, buffer[0]);

        // Check if this is the EOF packet (first byte is 0xFF)
        if (buffer[0] == (char)255)
        {
            printf("Received EOF packet. File transfer complete.\n");
            break;
        }

        buffer[bytes_received] = '\0';

        // Write the received data to the file
        size_t bytes_written = fprintf(file, "%s", buffer);

        printf("Received packet %d (%zd bytes)\n", packets_received, bytes_written);
    }

    printf("File reception complete. Received %d packets.\n", packets_received);

    // Clean up
    k_close(sock);
    fclose(file);

    return 0;
}