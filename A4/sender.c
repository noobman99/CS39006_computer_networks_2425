#include <stdio.h>
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
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    // Open the input file
    FILE *file = fopen(argv[1], "r");
    if (!file)
    {
        perror("Error opening input file");
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

    // Set up the server address
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
    if (k_bind(sock, (struct sockaddr *)&client_addr, sizeof(client_addr),
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        k_close(sock);
        fclose(file);
        exit(1);
    }

    printf("Socket bound successfully. Starting to send file...\n");

    // Buffer for reading from file
    char buffer[MESSAGE_SIZE];
    memset(buffer, 0, MESSAGE_SIZE);

    // Read and send the file in chunks
    size_t bytes_read;
    int packets_sent = 0;

    while ((bytes_read = fread(buffer, 1, MESSAGE_SIZE, file)) > 0)
    {
        // If we read less than MESSAGE_SIZE bytes, pad the buffer with zeros
        if (bytes_read < MESSAGE_SIZE)
        {
            memset(buffer + bytes_read, 0, MESSAGE_SIZE - bytes_read);
        }

        // Send the chunk
        while (k_sendto(sock, buffer, MESSAGE_SIZE, 0,
                        (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            if (errno != ENOSPACE)
            {
                k_close(sock);
                fclose(file);
                return 1;
            }
            perror("Waiting to send further data");
            sleep(1); // 100 milliseconds sleep
        }

        packets_sent++;
        printf("Sent packet %d (%zu bytes)\n", packets_sent, bytes_read);

        // Clear the buffer for the next read
        memset(buffer, 0, MESSAGE_SIZE);

        // Small delay to avoid overwhelming the receiver
        usleep(10000); // 10 milliseconds
    }

    // Send one final zero-filled packet to indicate end of file
    if (bytes_read == 0 && !feof(file))
    {
        perror("Error reading from file");
    }
    else
    {
        memset(buffer, (char)255, MESSAGE_SIZE);

        // First byte indicates this is the EOF packet

        while (k_sendto(sock, buffer, MESSAGE_SIZE, 0,
                        (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            if (errno != ENOSPACE)
            {
                k_close(sock);
                fclose(file);
                return 1;
            }

            perror("Error sending EOF packet");
            sleep(1);
            continue;
        }
    }

    printf("File transmission complete. Sent %d packets.\n", packets_sent);
    printf("Waiting for everything to be sent\n");

    // sleep(10);

    while (k_isempty(sock) == 0)
    {
        sleep(1);
    }

    // Clean up
    k_close(sock);
    fclose(file);

    return 0;
}