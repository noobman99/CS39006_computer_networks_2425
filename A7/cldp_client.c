/*
=====================================
Assignment 7 Submission
Name: Parth Shashin Patil
Roll number: 22CS30041
=====================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>

#define CLDP_PROTOCOL_NUM 253
#define HELLO 0x01
#define QUERY 0x02
#define RESPONSE 0x03

#define HOSTNAME_LENGTH 64
#define CPU_LOAD_LENGTH 10
#define TIME_LENGTH 20

#define BUFFER_SIZE 1500
#define SERVER_TIMEOUT 30
#define TIMEOUT_CHECK_INTERVAL 5
#define QUERY_INTERVAL 10
#define MAX_SERVERS 32

// CLDP header structure (8 bytes)
typedef struct __attribute__((packed))
{
    uint8_t msg_type;       // 0x01: HELLO, 0x02: QUERY, 0x03: RESPONSE
    uint8_t payload_length; // Length (in bytes) of the payload
    uint16_t trans_id;      // Transaction ID
    uint32_t reserved;      // Reserved field (set to 0)
} cldp_header_t;

// Server entry structure
typedef struct server_entry
{
    struct sockaddr_in addr;
    time_t last_seen;
    int active;
    time_t last_response_time;
} server_entry_t;

// Global variables
int raw_sock = -1;
int running = 1;
pthread_t query_thread_id;
server_entry_t servers[MAX_SERVERS];
pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
uint16_t next_trans_id = 1;

// Function prototypes
void *query_sender(void *arg);
void handle_hello(char *buffer, ssize_t bytes, struct sockaddr_in *src_addr);
void handle_response(char *buffer, ssize_t bytes, struct sockaddr_in *src_addr);
void check_timeouts();
void send_query(server_entry_t *server);
unsigned short compute_checksum(unsigned short *buf, int nwords);
in_addr_t get_local_ip();
void signal_handler(int signum);

// Get the local IP address
in_addr_t get_local_ip()
{
    struct ifaddrs *ifaddr, *ifa;
    in_addr_t ip = htonl(INADDR_LOOPBACK);

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs failed");
        return ip;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, "lo") != 0)
        {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            ip = addr->sin_addr.s_addr;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return ip;
}

// Compute checksum for IP header
unsigned short compute_checksum(unsigned short *buf, int nwords)
{
    unsigned long sum = 0;
    for (int i = 0; i < nwords; i++)
    {
        sum += buf[i];
    }
    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (unsigned short)(~sum);
}

// Signal handler for graceful shutdown
void signal_handler(int signum)
{
    printf("\nReceived signal %d. Shutting down...\n", signum);
    running = 0;
    if (raw_sock != -1)
    {
        close(raw_sock);
    }
    pthread_cancel(query_thread_id);
    exit(0);
}

// Handle HELLO message
void handle_hello(char *buffer, ssize_t bytes, struct sockaddr_in *src_addr)
{
    struct iphdr *ip = (struct iphdr *)buffer;
    int ip_header_len = ip->ihl * 4;
    cldp_header_t *cldp = (cldp_header_t *)(buffer + ip_header_len);

    if (bytes < ip_header_len + sizeof(cldp_header_t))
    {
        printf("Received malformed packet: too short\n");
        return;
    }

    if (cldp->msg_type != HELLO)
    {
        return; // Not a HELLO message
    }

    // Convert source IP to string
    char src_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(src_addr->sin_addr), src_ip, INET_ADDRSTRLEN);

    // Update server list
    pthread_mutex_lock(&server_mutex);

    int found = 0;
    for (int i = 0; i < MAX_SERVERS; i++)
    {
        if (servers[i].active &&
            servers[i].addr.sin_addr.s_addr == src_addr->sin_addr.s_addr)
        {
            // Update existing server
            servers[i].last_seen = time(NULL);
            found = 1;
            break;
        }
    }

    if (!found)
    {
        // Add new server
        for (int i = 0; i < MAX_SERVERS; i++)
        {
            if (!servers[i].active)
            {
                servers[i].addr = *src_addr;
                servers[i].last_seen = time(NULL);
                servers[i].active = 1;
                servers[i].last_response_time = 0;
                printf("Added new server: %s\n", src_ip);
                break;
            }
        }
    }

    pthread_mutex_unlock(&server_mutex);
}

// Handle RESPONSE message
void handle_response(char *buffer, ssize_t bytes, struct sockaddr_in *src_addr)
{
    struct iphdr *ip = (struct iphdr *)buffer;
    int ip_header_len = ip->ihl * 4;
    cldp_header_t *cldp = (cldp_header_t *)(buffer + ip_header_len);
    char *hostname, *time_str, *cpu_load;
    char payload[256];

    memset(payload, 0, sizeof(payload));

    if (bytes < ip_header_len + sizeof(cldp_header_t))
    {
        printf("Received malformed packet: too short\n");
        return;
    }

    if (cldp->msg_type != RESPONSE)
    {
        return; // Not a RESPONSE message
    }

    // Extract payload
    int payload_len = cldp->payload_length;
    if (bytes < ip_header_len + sizeof(cldp_header_t) + payload_len)
    {
        printf("Received malformed packet: payload too short\n");
        return;
    }

    memcpy(payload, buffer + ip_header_len + sizeof(cldp_header_t), payload_len);

    hostname = payload;
    time_str = payload + HOSTNAME_LENGTH;
    cpu_load = payload + HOSTNAME_LENGTH + TIME_LENGTH;

    // Convert source IP to string
    char src_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(src_addr->sin_addr), src_ip, INET_ADDRSTRLEN);

    // Update server list
    pthread_mutex_lock(&server_mutex);

    for (int i = 0; i < MAX_SERVERS; i++)
    {
        if (servers[i].active &&
            servers[i].addr.sin_addr.s_addr == src_addr->sin_addr.s_addr)
        {
            // Update server response
            servers[i].last_response_time = time(NULL);
            printf("Received RESPONSE from %s (trans_id: %u): Hostname: %s, Time: %s, CPU Load: %s%%\n",
                   src_ip, ntohs(cldp->trans_id), hostname, time_str, cpu_load);
            break;
        }
    }

    pthread_mutex_unlock(&server_mutex);
}

uint16_t get_rand_id()
{
    uint16_t t = rand() % (1 << 16);
    return t;
}

void set_ip_header(struct iphdr *ip)
{
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->id = htons(get_rand_id());
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = CLDP_PROTOCOL_NUM;
    ip->check = 0;
    ip->saddr = get_local_ip();
}

// Send QUERY to a server
void send_query(server_entry_t *server)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Construct IP header
    struct iphdr *ip = (struct iphdr *)buffer;
    cldp_header_t *cldp = (cldp_header_t *)(buffer + sizeof(struct iphdr));

    // Fill in CLDP header
    cldp->msg_type = QUERY;
    cldp->payload_length = 0;
    cldp->trans_id = htons(next_trans_id++);
    cldp->reserved = 0;

    // Fill in IP header
    set_ip_header(ip);
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(cldp_header_t));
    ip->daddr = server->addr.sin_addr.s_addr;

    // Compute IP header checksum
    ip->check = compute_checksum((unsigned short *)ip, ip->ihl * 2);

    // Send QUERY
    if (sendto(raw_sock, buffer, ntohs(ip->tot_len), 0,
               (struct sockaddr *)&(server->addr), sizeof(server->addr)) < 0)
    {
        perror("sendto QUERY failed");
    }
    else
    {
        char dest_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(server->addr.sin_addr), dest_ip, INET_ADDRSTRLEN);
        printf("Sent QUERY to %s (trans_id: %u)\n", dest_ip, ntohs(cldp->trans_id));
    }
}

// Check for server timeouts
void check_timeouts()
{
    time_t now = time(NULL);

    pthread_mutex_lock(&server_mutex);

    for (int i = 0; i < MAX_SERVERS; i++)
    {
        if (servers[i].active && (now - servers[i].last_seen > SERVER_TIMEOUT))
        {
            // Server timed out
            char server_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(servers[i].addr.sin_addr), server_ip, INET_ADDRSTRLEN);
            printf("Server %s timed out\n", server_ip);
            servers[i].active = 0;
        }
    }

    pthread_mutex_unlock(&server_mutex);
}

// Thread to send QUERY messages periodically
void *query_sender(void *arg)
{
    while (running)
    {
        pthread_mutex_lock(&server_mutex);

        // Count active servers
        int active_count = 0;
        for (int i = 0; i < MAX_SERVERS; i++)
        {
            if (servers[i].active)
            {
                active_count++;
            }
        }

        if (active_count == 0)
        {
            printf("No active servers to query\n");
        }
        else
        {
            // Send QUERY to all active servers
            for (int i = 0; i < MAX_SERVERS; i++)
            {
                if (servers[i].active)
                {
                    send_query(&servers[i]);
                }
            }
        }

        pthread_mutex_unlock(&server_mutex);

        // Sleep for QUERY_INTERVAL seconds
        sleep(QUERY_INTERVAL);
    }
    return NULL;
}

int main()
{
    printf("CLDP Client starting up...\n");

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize random number generator
    srand(time(NULL));

    // Initialize server list
    pthread_mutex_lock(&server_mutex);
    for (int i = 0; i < MAX_SERVERS; i++)
    {
        servers[i].active = 0;
    }
    pthread_mutex_unlock(&server_mutex);

    // Create raw socket
    if ((raw_sock = socket(AF_INET, SOCK_RAW, CLDP_PROTOCOL_NUM)) < 0)
    {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int one = 1;
    // IP_HDRINCL: We'll provide the IP header
    if (setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0)
    {
        perror("setsockopt IP_HDRINCL failed");
        close(raw_sock);
        exit(EXIT_FAILURE);
    }

    // Create thread for sending QUERY messages
    if (pthread_create(&query_thread_id, NULL, query_sender, NULL) != 0)
    {
        perror("pthread_create failed");
        close(raw_sock);
        exit(EXIT_FAILURE);
    }

    // Set up for select()
    fd_set readfds;
    struct timeval tv;
    int max_fd = raw_sock;

    printf("Listening for server announcements...\n");

    // Main loop
    while (running)
    {
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);

        // Reset file descriptor set
        FD_ZERO(&readfds);
        FD_SET(raw_sock, &readfds);

        // Set timeout
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        // Wait for activity on the socket
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);

        if (activity < 0 && errno != EINTR)
        {
            perror("select error");
            continue;
        }

        // Check for timeout
        if (activity == 0)
        {
            // Check for server timeouts
            check_timeouts();
            continue; // No activity, loop back
        }

        // Check if we have data on the socket
        if (FD_ISSET(raw_sock, &readfds))
        {
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);

            // Receive packet
            ssize_t bytes = recvfrom(raw_sock, buffer, BUFFER_SIZE, 0,
                                     (struct sockaddr *)&src_addr, &addrlen);

            if (bytes < 0)
            {
                if (errno != ECONNREFUSED)
                {
                    perror("recvfrom failed");
                }
                continue;
            }

            // Validate packet
            if (bytes < sizeof(struct iphdr))
            {
                printf("Received malformed packet: too short for IP header\n");
                continue;
            }

            struct iphdr *ip = (struct iphdr *)buffer;

            // Check if it's our protocol
            if (ip->protocol != CLDP_PROTOCOL_NUM)
            {
                continue; // Not our protocol
            }

            // Handle HELLO and RESPONSE messages
            int ip_header_len = ip->ihl * 4;
            if (bytes < ip_header_len + sizeof(cldp_header_t))
            {
                printf("Received malformed packet: too short for CLDP header\n");
                continue;
            }

            cldp_header_t *cldp = (cldp_header_t *)(buffer + ip_header_len);

            if (cldp->msg_type == HELLO)
            {
                handle_hello(buffer, bytes, &src_addr);
            }
            else if (cldp->msg_type == RESPONSE)
            {
                handle_response(buffer, bytes, &src_addr);
            }
        }
    }

    // Clean up
    pthread_join(query_thread_id, NULL);
    close(raw_sock);

    printf("Client shutdown complete.\n");
    return 0;
}