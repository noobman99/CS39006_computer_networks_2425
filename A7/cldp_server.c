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
#include <sys/sysinfo.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <signal.h>

#define CLDP_PROTOCOL_NUM 253
#define HELLO 0x01
#define QUERY 0x02
#define RESPONSE 0x03

#define HOSTNAME_LENGTH 64
#define CPU_LOAD_LENGTH 10
#define TIME_LENGTH 20

#define BUFFER_SIZE 1500
#define HELLO_INTERVAL 10
#define MAX_CLIENTS 16

// CLDP header structure (8 bytes)
typedef struct __attribute__((packed))
{
    uint8_t msg_type;       // 0x01: HELLO, 0x02: QUERY, 0x03: RESPONSE
    uint8_t payload_length; // Length (in bytes) of the payload
    uint16_t trans_id;      // Transaction ID
    uint32_t reserved;      // Reserved field (set to 0)
} cldp_header_t;

// Global variables
int raw_sock = -1;
int running = 1;
pthread_t hello_thread_id;

void *hello_broadcaster(void *arg);
void handle_client_query(char *buffer, ssize_t bytes, struct sockaddr_in *src_addr);
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
    pthread_cancel(hello_thread_id);
    exit(0);
}

uint16_t get_rand_id()
{
    uint16_t t = rand() % (1 << 16 - 1);
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

// Thread to broadcast HELLO messages periodically
void *hello_broadcaster(void *arg)
{
    char buffer[BUFFER_SIZE];
    struct iphdr *ip;
    cldp_header_t *cldp;
    struct sockaddr_in dest;

    while (running)
    {
        memset(buffer, 0, BUFFER_SIZE);

        ip = (struct iphdr *)buffer;
        cldp = (cldp_header_t *)(buffer + sizeof(struct iphdr));

        // Fill in CLDP header
        cldp->msg_type = HELLO;
        cldp->payload_length = 0;
        cldp->trans_id = htons(0); // No transaction ID
        cldp->reserved = 0;

        // Fill in IP header
        set_ip_header(ip);
        ip->tot_len = htons(sizeof(struct iphdr) + sizeof(cldp_header_t));
        ip->daddr = inet_addr("255.255.255.255"); // broadcast to all hosts

        // Compute IP header checksum
        ip->check = compute_checksum((unsigned short *)ip, ip->ihl * 2);

        // Set up destination address
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = ip->daddr;

        // Send HELLO message
        if (sendto(raw_sock, buffer, sizeof(struct iphdr) + sizeof(cldp_header_t), 0,
                   (struct sockaddr *)&dest, sizeof(dest)) < 0)
        {
            perror("sendto HELLO failed");
        }
        else
        {
            printf("Sent HELLO announcement\n");
        }

        // Sleep for HELLO_INTERVAL seconds
        sleep(HELLO_INTERVAL);
    }
    return NULL;
}

// Handle client QUERY and send RESPONSE
void handle_client_query(char *buffer, ssize_t bytes, struct sockaddr_in *src_addr)
{
    struct iphdr *ip, *r_ip;
    int ip_header_len;
    cldp_header_t *cldp, *r_cldp;
    char src_ip[INET_ADDRSTRLEN];
    char *hostname, *time_str, *cpu_load;
    char payload[256];
    struct sysinfo si;
    struct timeval tv;
    struct tm *tm;
    struct sockaddr_in dest;
    char response_buf[BUFFER_SIZE];
    int payload_len = 0;

    memset(payload, 0, sizeof(payload));

    hostname = payload;
    time_str = payload + HOSTNAME_LENGTH;
    cpu_load = payload + HOSTNAME_LENGTH + TIME_LENGTH;

    ip = (struct iphdr *)buffer;
    ip_header_len = ip->ihl * 4;
    cldp = (cldp_header_t *)(buffer + ip_header_len);

    if (bytes < ip_header_len + sizeof(cldp_header_t))
    {
        printf("Received malformed packet: too short\n");
        return;
    }

    if (cldp->msg_type != QUERY)
    {
        return; // Not a QUERY message
    }

    inet_ntop(AF_INET, &(ip->saddr), src_ip, INET_ADDRSTRLEN);
    printf("Received QUERY from %s, transaction id: %u\n", src_ip, ntohs(cldp->trans_id));

    // Gather system metadata
    // 1. Hostname
    if (gethostname(hostname, HOSTNAME_LENGTH) < 0)
    {
        perror("gethostname failed");
        return;
    }

    // 2. Current time
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);
    strftime(time_str, TIME_LENGTH, "%Y-%m-%d %H:%M:%S", tm);

    // 3. CPU load
    if (sysinfo(&si) < 0)
    {
        perror("sysinfo failed");
        return;
    }
    snprintf(cpu_load, CPU_LOAD_LENGTH, "%.2f", (double)si.loads[0] / (1 << SI_LOAD_SHIFT));

    payload_len = HOSTNAME_LENGTH + TIME_LENGTH + CPU_LOAD_LENGTH;

    // Create RESPONSE packet
    memset(response_buf, 0, BUFFER_SIZE);

    r_ip = (struct iphdr *)response_buf;
    r_cldp = (cldp_header_t *)(response_buf + sizeof(struct iphdr));

    // Fill in  headers
    r_cldp->msg_type = RESPONSE;
    r_cldp->payload_length = payload_len;
    r_cldp->trans_id = cldp->trans_id; // Use the same transaction ID
    r_cldp->reserved = 0;

    set_ip_header(r_ip);
    r_ip->tot_len = htons(sizeof(struct iphdr) + sizeof(cldp_header_t) + payload_len);
    r_ip->daddr = ip->saddr;

    r_ip->check = compute_checksum((unsigned short *)r_ip, r_ip->ihl * 2);

    // Copy payload
    memcpy(response_buf + sizeof(struct iphdr) + sizeof(cldp_header_t),
           payload, payload_len);

    // Set up destination address
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = r_ip->daddr;

    if (sendto(raw_sock, response_buf, sizeof(struct iphdr) + sizeof(cldp_header_t) + payload_len, 0,
               (struct sockaddr *)&dest, sizeof(dest)) < 0)
    {
        perror("sendto RESPONSE failed");
    }
    else
    {
        printf("Sent RESPONSE to %s\n", inet_ntoa(*(struct in_addr *)&r_ip->daddr));
    }
}

int main()
{
    printf("CLDP Server starting up...\n");

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize random number generator
    srand(time(NULL));

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

    // SO_BROADCAST: Allow sending to broadcast address
    if (setsockopt(raw_sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0)
    {
        perror("setsockopt SO_BROADCAST failed");
        close(raw_sock);
        exit(EXIT_FAILURE);
    }

    // Create thread for sending HELLO messages
    if (pthread_create(&hello_thread_id, NULL, hello_broadcaster, NULL) != 0)
    {
        perror("pthread_create failed");
        close(raw_sock);
        exit(EXIT_FAILURE);
    }

    // Set up for select()
    fd_set readfds;
    struct timeval tv;
    int max_fd = raw_sock;

    printf("Listening for incoming packets...\n");

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
                perror("recvfrom failed");
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

            // Handle client QUERY
            handle_client_query(buffer, bytes, &src_addr);
        }
    }

    // Clean up
    pthread_join(hello_thread_id, NULL);
    close(raw_sock);

    printf("Server shutdown complete.\n");
    return 0;
}