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
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>

#define PORT 2525
#define BUFFER_SIZE 1024
#define EMAIL_ID_SIZE 200
#define MAX_CLIENTS 10
#define MAX_EMAIL_SIZE 8192

typedef struct
{
    int client_socket;
    struct sockaddr_in client_address;
} client_info;

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

int read_message(int sock, char *buffer, int size)
{
    char buf[size];
    memset(buf, 0, size);
    memset(buffer, 0, size);

    while (1)
    {
        ssize_t bytes_received = recv(sock, buf, size - 1, 0);
        if (bytes_received <= 0)
        {
            perror("Receive failed");
            return -1;
        }
        strncat(buffer, buf, bytes_received);
        if (is_message_end(buf, bytes_received))
        {
            break;
        }
    }

    return 0;
}

void send_response(int client_socket, int code, const char *message, int is_header)
{
    char response[BUFFER_SIZE];
    sprintf(response, "%d %s\n", code, message);
    send(client_socket, response, strlen(response) + (is_header ? 1 : 0), 0);
}

void create_mailbox_dir()
{
    struct stat st = {0};
    if (stat("mailbox", &st) == -1)
    {
        if (mkdir("mailbox", 0700) == -1)
        {
            perror("Error creating mailbox directory");
            exit(EXIT_FAILURE);
        }
    }
}

void store_email(const char *from, const char *to, const char *body)
{
    // Create filename based on recipient email
    char filename[BUFFER_SIZE];
    sprintf(filename, "mailbox/%s.txt", to);

    // Get current date
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[20];
    strftime(date, sizeof(date), "%d-%m-%Y", t);

    // Open file in append mode
    FILE *file = fopen(filename, "a");
    if (file == NULL)
    {
        perror("Error opening file");
        return;
    }

    // Find next available ID
    int id = 1;
    FILE *temp = fopen(filename, "r");
    if (temp != NULL)
    {
        char line[BUFFER_SIZE];
        while (fgets(line, BUFFER_SIZE, temp) != NULL)
        {
            if (strncmp(line, "ID: ", 4) == 0)
            {
                int current_id;
                sscanf(line, "ID: %d", &current_id);
                if (current_id >= id)
                {
                    id = current_id + 1;
                }
            }
        }
        fclose(temp);
    }

    // Write email to file
    fprintf(file, "ID: %d\n", id);
    fprintf(file, "From: %s\n", from);
    fprintf(file, "Date: %s\n", date);
    fprintf(file, "%s\n", body);

    fclose(file);
}

void list_emails(int client_socket, const char *email)
{
    char filename[BUFFER_SIZE];
    sprintf(filename, "mailbox/%s.txt", email);

    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        if (errno == ENOENT)
        {
            // File doesn't exist - no emails
            send_response(client_socket, 200, "OK No emails found", 1);
        }
        else
        {
            send_response(client_socket, 500, "SERVER ERROR", 1);
        }
        return;
    }

    // Send OK response header
    send_response(client_socket, 200, "OK", 0);

    char line[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    int id;
    char from[EMAIL_ID_SIZE];
    char date[EMAIL_ID_SIZE];

    while (fgets(line, BUFFER_SIZE, file) != NULL)
    {
        memset(from, 0, BUFFER_SIZE);
        memset(date, 0, BUFFER_SIZE);
        memset(response, 0, BUFFER_SIZE);

        if (strncmp(line, "ID: ", 4) == 0)
        {
            sscanf(line, "ID: %d", &id);

            // Get From line
            if (fgets(line, BUFFER_SIZE, file) != NULL)
            {
                sscanf(line, "From: %s", from);
            }

            // Get Date line
            if (fgets(line, BUFFER_SIZE, file) != NULL)
            {
                sscanf(line, "Date: %s", date);
            }

            // Format and send email summary
            sprintf(response, "%d: Email from %s (%s)\n", id, from, date);
            send(client_socket, response, strlen(response), 0);
        }
    }

    fclose(file);
    printf("Emails retrieved; list sent.\n");

    // send end of list marker
    memset(response, 0, BUFFER_SIZE);
    response[0] = '.';
    response[1] = '\n';
    send(client_socket, response, 3, 0);
}

void get_email(int client_socket, const char *email, int id)
{
    char filename[BUFFER_SIZE];
    sprintf(filename, "mailbox/%s.txt", email);

    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        send_response(client_socket, 401, "NOT FOUND No emails for this recipient", 1);
        return;
    }

    char line[BUFFER_SIZE];
    int current_id = 0;
    int found = 0;
    int collecting = 0;

    while (fgets(line, BUFFER_SIZE, file) != NULL)
    {
        if (strncmp(line, "ID: ", 4) == 0)
        {
            sscanf(line, "ID: %d", &current_id);
            if (current_id == id)
            {
                found = 1;
                collecting = 1;
                send_response(client_socket, 200, "OK Email found", 0);
                send(client_socket, line, strlen(line), 0);
            }
            else if (collecting)
            {
                collecting = 0;
                break;
            }
        }
        else if (collecting)
        {
            if (strcmp(line, ".\n") == 0)
            {
                send(client_socket, line, strlen(line) + 1, 0);
                collecting = 0;
                printf("Email with ID %d sent.\n", id);
                break;
            }
            else
            {
                send(client_socket, line, strlen(line), 0);
            }
        }

        memset(line, 0, BUFFER_SIZE);
    }

    fclose(file);

    if (!found)
    {
        send_response(client_socket, 401, "NOT FOUND Email with specified ID does not exist", 1);
    }
}

void *handle_client(void *arg)
{
    client_info *client = (client_info *)arg;
    int client_socket = client->client_socket;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    char email_body[MAX_EMAIL_SIZE] = "";
    char client_id[BUFFER_SIZE];
    char sender[BUFFER_SIZE] = "";
    char recipient[BUFFER_SIZE] = "";
    int data_mode = 0;
    int is_connected = 0;

    memset(buffer, 0, BUFFER_SIZE);
    memset(message, 0, BUFFER_SIZE);
    memset(email_body, 0, MAX_EMAIL_SIZE);
    memset(sender, 0, BUFFER_SIZE);
    memset(recipient, 0, BUFFER_SIZE);
    memset(client_id, 0, BUFFER_SIZE);

    while (1)
    {
        // Read message from client
        if (
            read_message(client_socket, buffer, BUFFER_SIZE) < 0)
        {
            perror("Error reading message");
            break;
        }

        // If in DATA mode, collect the message body
        if (data_mode)
        {
            strcat(email_body, buffer);
            strcat(email_body, "\n");

            if (strcmp(buffer, ".") == 0)
            {
                data_mode = 0;
                store_email(sender, recipient, email_body);
                printf("DATA received, message stored.\n");
                send_response(client_socket, 200, "Message stored successfully", 1);
                memset(email_body, 0, MAX_EMAIL_SIZE);
            }
            continue;
        }

        // Parse commands
        if (strncmp(buffer, "HELO ", 5) == 0)
        {
            sscanf(buffer, "HELO %s", client_id);
            printf("HELO received from %s\n", client_id);
            send_response(client_socket, 200, "OK", 1);
            is_connected = 1;
        }
        else if (strncmp(buffer, "MAIL FROM: ", 11) == 0)
        {
            if (!is_connected)
            {
                send_response(client_socket, 403, "ERR HELO command required first", 1);
                continue;
            }
            sscanf(buffer, "MAIL FROM: %s", sender);
            printf("MAIL FROM: %s\n", sender);
            send_response(client_socket, 200, "OK", 1);
        }
        else if (strncmp(buffer, "RCPT TO: ", 9) == 0)
        {
            if (!is_connected)
            {
                send_response(client_socket, 403, "ERR HELO command required first", 1);
                continue;
            }
            sscanf(buffer, "RCPT TO: %s", recipient);
            printf("RCPT TO: %s\n", recipient);
            send_response(client_socket, 200, "OK", 1);
        }
        else if (strcmp(buffer, "DATA") == 0)
        {
            if (!is_connected)
            {
                send_response(client_socket, 403, "ERR HELO command required first", 1);
                continue;
            }

            if (strlen(sender) == 0 || strlen(recipient) == 0)
            {
                send_response(client_socket, 403, "ERR Sender and recipient must be specified", 1);
            }
            else
            {
                data_mode = 1;
                send_response(client_socket, 200, "OK", 1);
            }
        }
        else if (strncmp(buffer, "LIST ", 5) == 0)
        {
            if (!is_connected)
            {
                send_response(client_socket, 403, "ERR HELO command required first", 1);
                continue;
            }

            char email[BUFFER_SIZE];
            sscanf(buffer, "LIST %s", email);
            printf("LIST %s\n", email);
            list_emails(client_socket, email);
        }
        else if (strncmp(buffer, "GET_MAIL ", 9) == 0)
        {
            if (!is_connected)
            {
                send_response(client_socket, 403, "ERR HELO command required first", 1);
                continue;
            }

            char email[BUFFER_SIZE];
            int id;
            sscanf(buffer, "GET_MAIL %s %d", email, &id);
            printf("GET_MAIL %s %d\n", email, id);
            get_email(client_socket, email, id);
        }
        else if (strcmp(buffer, "QUIT") == 0)
        {

            send_response(client_socket, 200, "Goodbye", 1);
            printf("Client disconnected.\n");
            break;
        }
        else
        {
            send_response(client_socket, 400, "ERR Invalid command", 1);
        }
    }

    close(client_socket);
    free(client);
    return NULL;
}

int main(int argc, char *argv[])
{
    int server_socket, client_socket, addr_size;
    struct sockaddr_in server_addr, client_addr;
    pthread_t thread_id;

    // Check if port is provided
    int port = PORT;
    if (argc > 1)
    {
        port = atoi(argv[1]);
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Error in socket creation");
        exit(1);
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("Error in setsockopt");
        exit(1);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket to address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error in binding");
        exit(1);
    }

    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0)
    {
        perror("Error in listening");
        exit(1);
    }

    // Create mailbox directory
    create_mailbox_dir();

    printf("Listening on port %d...\n", port);

    while (1)
    {
        // Accept connection
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, (socklen_t *)&addr_size);

        if (client_socket < 0)
        {
            perror("Error in accepting connection");
            continue;
        }

        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        // Prepare client information
        client_info *client = malloc(sizeof(client_info));
        client->client_socket = client_socket;
        client->client_address = client_addr;

        // Create thread to handle client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client) != 0)
        {
            perror("Error creating thread");
            close(client_socket);
            free(client);
        }
        else
        {
            pthread_detach(thread_id);
        }
    }

    close(server_socket);
    return 0;
}
