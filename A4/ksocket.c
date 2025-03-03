// =====================================
// Assignment 3 Submission
// Name: Parth Shashin Patil
// Roll number: 22CS30041
// =====================================

#include "ksocket.h"
#include <stdio.h>

extern int errno;

// auxilary functions

// Simulate the packet drop with given probability
int dropmessage()
{
    double e = (rand() / (double)RAND_MAX);
    return e <= p;
}

// get the socket store from the shared memory
ktp_socket_store *get_socket_store()
{
    key_t shmkey = ftok(FKEY, KTP_PROJECT);
    int shmid = shmget(shmkey, sizeof(ktp_socket_store), 0666 | IPC_CREAT);

    if (shmid < 0)
    {
        errno = NOSOCKETRUNNING;
        exit(1);
    }

    ktp_socket_store *socket_store = shmat(shmid, NULL, 0);

    return socket_store;
}

// get the ktp_socket structure from the socket store with given index
ktp_socket *get_socket(ktp_sockid i)
{
    if (i >= MAX_SOCKETS)
    {
        return NULL;
    }

    ktp_socket_store *stor = get_socket_store();

    return &(stor->socks[i]);
}

// clear the socket data, make it ready for reuse
void clear_socket(ktp_socket *ksock)
{
    ksock->ip = 0;
    ksock->is_allocated = 0;
    ksock->is_terminated = 0;
    ksock->pid = 0;
    close(ksock->sockfd);
    ksock->sockfd = -1;
    ksock->port = 0;

    ksock->rwnd.ack_num = -1;
    ksock->rwnd.front = 0;
    ksock->rwnd.back = MAX_MESSAGES;
    ksock->rwnd.flags = 0;
    ksock->rwnd.max_size = MAX_MESSAGES;

    ksock->swnd.ack_num = -1;
    ksock->swnd.front = 0;
    ksock->swnd.back = 0;
    ksock->swnd.flags = 0;
    ksock->swnd.max_size = MAX_MESSAGES;

    ksock->recv_buf.size = 0;
    ksock->recv_buf.ptr = 0;
    memset(ksock->recv_buf.occ, 0, MAX_MESSAGES);
    for (int i = 0; i < MAX_MESSAGES; i++)
    {
        memset(ksock->recv_buf.s[i], 0, MESSAGE_SIZE);
    }

    ksock->send_buf.size = 0;
    ksock->send_buf.ptr = 0;
    memset(ksock->send_buf.occ, 0, MAX_MESSAGES);
    for (int i = 0; i < MAX_MESSAGES; i++)
    {
        memset(ksock->send_buf.s[i], 0, MESSAGE_SIZE);
    }
}

// check if the sequence number is in the window
int in_window(int seq_num, struct sld_wnd *wnd)
{
    if (wnd->back == wnd->front || seq_num < 0 || seq_num >= MAX_SEQ_NUMBER)
    {
        return 0;
    }

    if (wnd->back < wnd->front)
    {
        return (seq_num < wnd->back) || (seq_num >= wnd->front);
    }
    else
    {
        return seq_num < wnd->back && seq_num >= wnd->front;
    }
}

// increase the window (increase the back pointer)
int increase_window(struct sld_wnd *wnd)
{
    wnd->back = (wnd->back + 1) % MAX_SEQ_NUMBER;
    return wnd->back;
}

// decrease the window (increase the front pointer)
int decrease_window(struct sld_wnd *wnd)
{
    wnd->front = (wnd->front + 1) % MAX_SEQ_NUMBER;
    return wnd->front;
}

// get the size of the window (number of packets in the window)
int get_window_size(struct sld_wnd *wnd)
{
    if (wnd->back < wnd->front)
    {
        return (wnd->back - 0) + (MAX_SEQ_NUMBER - wnd->front);
    }
    else
    {
        return (wnd->back - wnd->front);
    }
}

// user functions

// allocate a socket from the socket store and return the socket id (index)
ktp_sockid k_socket(int __domain, int __type, int __protocol)
{
    ktp_socket_store *socket_store = get_socket_store();
    sem_t *mutex;

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        mutex = sem_open(socket_store->socks[i].mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

        sem_wait(mutex);

        if (socket_store->socks[i].is_allocated)
        {
            sem_post(mutex);
            sem_close(mutex);
            continue;
        }

        socket_store->socks[i].is_allocated = 1;
        socket_store->socks[i].pid = getpid();

        sem_post(mutex);
        sem_close(mutex);

        return i;
    }

    errno = ENOSPACE;
    return -1;
}

// bind the socket to the given source and destination addresses
int k_bind(ktp_sockid sock, SOCK_ADDR __src_addr, socklen_t __src_addr_len, SOCK_ADDR __cli_addr, socklen_t __cli_addr_len)
{
    ktp_socket *ksock = get_socket(sock);
    sem_t *mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);
    int ret;

    if (ksock == NULL)
    {
        errno = INVALIDSOCKET;
        return -1;
    }

    sem_wait(mutex);

    if (ksock->is_allocated == 0)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = INVALIDSOCKET;
        return -1;
    }

    struct sockaddr_in *t = (struct sockaddr_in *)__cli_addr;

    ksock->ip = t->sin_addr.s_addr;
    ksock->port = t->sin_port;

    t = (struct sockaddr_in *)__src_addr;

    ksock->s_ip = t->sin_addr.s_addr;
    ksock->s_port = t->sin_port;

    sem_post(mutex);
    sem_close(mutex);
    return ret;
}

// Puts the given message in the send buffer of the socket and returns the number of bytes sent (NON-BLOCKING)
ssize_t k_sendto(ktp_sockid sock, void *buf, size_t n, int flags, SOCK_ADDR __addr, socklen_t __addr_len)
{
    ktp_socket *ksock = get_socket(sock);
    sem_t *mutex;
    int seqno;
    char *_buf;
    struct sockaddr_in *t = (struct sockaddr_in *)__addr;

    if (ksock == NULL)
    {
        errno = INVALIDSOCKET;
        perror("Invalid socket descriptor");
        return -1;
    }

    if (n != MESSAGE_SIZE)
    {
        perror("Message size should be 512 bytes");
        return -1;
    }

    _buf = (char *)buf;

    mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    sem_wait(mutex);

    if (ksock->is_allocated == 0)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = INVALIDSOCKET;
        perror("Socket is unallocated");
        return -1;
    }

    if (t->sin_addr.s_addr != ksock->ip || t->sin_port != ksock->port)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = ENOTBOUND;
        perror("Dst addr != bound address");
        return -1;
    }

    if (ksock->send_buf.size == MAX_MESSAGES)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = ENOSPACE;
        perror("Send buffer is full");
        return -1;
    }

    seqno = ksock->send_buf.ptr;

    for (int i = 0; i < MESSAGE_SIZE; i++)
    {
        ksock->send_buf.s[seqno][i] = _buf[i];
    }
    ksock->send_buf.occ[seqno] = 1;

    ksock->send_buf.size++;
    ksock->send_buf.ptr = (seqno + 1) % MAX_MESSAGES;

    sem_post(mutex);
    sem_close(mutex);

    return n;
}

// Reads the message from the recieve buffer of the socket and returns the number of bytes read (NON-BLOCKING)
ssize_t k_recvfrom(ktp_sockid sock, void *buf, size_t n, int flags, SOCK_ADDR __addr, socklen_t *__addr_len)
{
    ktp_socket *ksock = get_socket(sock);
    sem_t *mutex;
    int ret, seqno;
    char *_buf;
    struct sockaddr_in *t = (struct sockaddr_in *)__addr;

    if (ksock == NULL)
    {
        errno = INVALIDSOCKET;
        perror("Invalid socket descriptor");
        return -1;
    }

    if (n != MESSAGE_SIZE)
    {
        perror("Message size should be 512 bytes");
        return -1;
    }

    mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    sem_wait(mutex);

    if (ksock->is_allocated == 0)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = INVALIDSOCKET;
        perror("Socket is unallocated");
        return -1;
    }

    seqno = ksock->recv_buf.ptr;

    if (ksock->recv_buf.size == 0 || ksock->recv_buf.occ[seqno] == 0)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = ENOMESSAGE;
        perror("Recieve buffer is empty");
        return -1;
    }

    _buf = ksock->recv_buf.s[seqno];

    for (int i = 0; i < n; i++)
    {
        ((char *)buf)[i] = _buf[i];
    }

    // update that recv buff has been emptied for given message
    ksock->recv_buf.occ[seqno] = 0;
    memset(ksock->recv_buf.s[seqno], 0, MESSAGE_SIZE);

    ksock->recv_buf.size--;
    ksock->recv_buf.ptr = (seqno + 1) % MAX_MESSAGES;

    // also increase the pointer for recieve window
    increase_window(&ksock->rwnd);

    sem_post(mutex);
    sem_close(mutex);

    return n;
}

// Check if the socket has any pending operations to be done.
// If true, socket is safe to close
int k_isempty(ktp_sockid sock)
{
    ktp_socket *ksock = get_socket(sock);

    if (ksock == NULL)
    {
        return -1;
    }

    sem_t *mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    sem_wait(mutex);

    int isempty = (ksock->recv_buf.size == 0) && (ksock->send_buf.size == 0);

    sem_post(mutex);

    return isempty;
}

// Unallocate the socket and free the resources
int k_close(ktp_sockid sock)
{
    ktp_socket_store *socket_store = get_socket_store();
    ktp_socket *ksock = get_socket(sock);

    if (ksock == NULL)
    {
        return 0;
    }

    sem_t *mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    sem_wait(mutex);

    ksock->is_terminated = 1;

    sem_post(mutex);
    sem_close(mutex);
}
