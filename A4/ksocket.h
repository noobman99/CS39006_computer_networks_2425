#ifndef KTP_SOCKET
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h> /* For O_* constants */
#include <sys/stat.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/shm.h>
#define KTP_SOCKET 0
#endif

#define MESSAGE_SIZE 512
#define MAX_MESSAGES 10
#define MAX_SOCKETS 2
#define HEADER_SIZE 4
#define MAX_SEQ_NUMBER 120
#define MESSAGE_TIMEOUT 10
#define RECIEVE_TIMEOUT 5
#define GC_TIMEOUT 2
#define SEM_FLAGS (O_CREAT)
#define SEM_PERMS 0666
#define SEM_INITVAL 1
#define p 0

#define SOCK_KTP 0101

#define SOCK_ADDR struct sockaddr *

#define KTP_PROJECT 9
#define KTP_SEM_PROJ "ktpsemaa"
#define FKEY "/home"

#define NOSOCKETRUNNING 0
#define ENOSPACE 1
#define NOTBINDABLE 2
#define INVALIDSOCKET 3
#define ENOTBOUND 4
#define ENOMESSAGE 5
#define EXISTINGHANDLER 6

// flags
#define NOSPACE 1

typedef int ktp_sockid;
typedef int semaphore;

struct sld_wnd
{
    int max_size;
    int front;
    int back;
    int ack_num;
    int flags;
};

struct sck_buf
{
    char s[MAX_MESSAGES][MESSAGE_SIZE];
    int occ[MAX_MESSAGES];
    int ptr;
    int size;
};

struct ktp_socket_info
{
    int is_allocated;
    int pid;
    int sockfd;
    in_addr_t ip;
    in_port_t port;
    in_addr_t s_ip;
    in_port_t s_port;
    char mutex[10];
    struct sck_buf send_buf;
    struct sck_buf recv_buf;
    struct sld_wnd swnd;
    struct sld_wnd rwnd;
};

struct ksocket
{
    sem_t mutex;
    struct ktp_socket_info socks[MAX_SOCKETS];
};

typedef struct ksocket ktp_socket_store;
typedef struct ktp_socket_info ktp_socket;

// auxilary functions
int dropmessage();
ktp_socket_store *get_socket_store();
ktp_socket *get_socket(ktp_sockid i);
void clear_socket(ktp_socket *ksock);
int in_window(int seq_num, struct sld_wnd *wnd);
int increase_window(struct sld_wnd *wnd);
int decrease_window(struct sld_wnd *wnd);
int get_window_size(struct sld_wnd *wnd);
int set_flag(int __flag, struct sld_wnd *wnd);

// user functions

ktp_sockid k_socket(int __domain, int __type, int __protocol);
int k_bind(ktp_sockid sock, SOCK_ADDR __src_addr, socklen_t __src_addr_len, SOCK_ADDR __cli_addr, socklen_t __cli_addr_len);
ssize_t k_sendto(ktp_sockid sock, void *buf, size_t n, int flags, SOCK_ADDR __addr, socklen_t __addr_len);
ssize_t k_recvfrom(ktp_sockid sock, void *buf, size_t n, int flags, SOCK_ADDR __addr, socklen_t *__addr_len);
int k_isempty(ktp_sockid sock);
int k_close(ktp_sockid sock);