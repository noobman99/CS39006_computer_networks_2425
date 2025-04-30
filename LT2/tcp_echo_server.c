#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 9090
#define RECV_BUFFER_SIZE 8

void set_socket_options(int sockfd) {
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
            &yes, sizeof(yes)) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(1);
    }

    int size = 8 * 1204;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
            &size, sizeof(size)) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(1);
    }

    int size_out;
    socklen_t ss;
    ss = sizeof(size_out);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
            &size_out, &ss) == -1) {
        perror("setsockopt");
        close(sockfd);
        exit(1);
    }

    printf("Size of recv buf for socket %d is %d\n", sockfd, size_out);
}

void handle_client(int client_fd) {
    int n;
    char buff[RECV_BUFFER_SIZE];

    struct sockaddr_in cli_addr;
    socklen_t addr_len;
    addr_len = sizeof(cli_addr);
    memset(&cli_addr, 0, sizeof(cli_addr));

    getpeername(client_fd,  (struct sockaddr *) &cli_addr, &addr_len);

    set_socket_options(client_fd);
    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    printf("Client from <%s:%d> has joined.\n", inet_ntoa(*(struct in_addr *) &cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));

    while (1) {
        memset(buff, 0, sizeof(buff));
        n = recv(client_fd, buff, RECV_BUFFER_SIZE, 0);

        if (n == 0) {
            printf("Client from <%s:%d> has disconnected normally.\n", inet_ntoa(*(struct in_addr *) &cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));
            close(client_fd);
            return;
        }

    // The given problem statement would cause server to exit immediately as it hits EWOULDBLOCK whenever client is not sending messages. 
    // Thus to fix that Implemented that if error is EWOULDBLOCK then continue else client disconnected abruptly
        if (n < 0) {
            if (errno == EWOULDBLOCK) {
                continue;
            } else {
                printf("Client from <%s:%d> has disconnected abruptly.\n", inet_ntoa(*(struct in_addr *) &cli_addr.sin_addr.s_addr), ntohs(cli_addr.sin_port));
                close(client_fd);
                return;
            }
        }

        send(client_fd, buff, n, 0);
    }
}

int create_server_socket(int port) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;

    int sockfd;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    set_socket_options(sockfd);

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}

int main() {
    struct sockaddr_in cli_addr;

    int sockfd, client_sockfd;

    sockfd = create_server_socket(PORT);

    listen(sockfd, 10);

    printf("Server listening on port %d\n", PORT);

    while (1) {
        socklen_t addr_len = sizeof(cli_addr);
        client_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &addr_len);
        if (client_sockfd < 1) {
            perror("accept");
            close(sockfd);
            return -1;
        }

        if (fork() == 0) {
            close(sockfd);
            handle_client(client_sockfd);
            exit(0);
        }

        close(client_sockfd);
    }


}