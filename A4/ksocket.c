#include "ksocket.h"
#include <stdio.h>

extern int errno;

// auxilary functions

int dropmessage()
{
    double e = (rand() / (double)RAND_MAX);
    return e <= p;
}

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

ktp_socket *get_socket(ktp_sockid i)
{
    if (i >= MAX_SOCKETS)
    {
        return NULL;
    }

    ktp_socket_store *stor = get_socket_store();

    return &(stor->socks[i]);
}

void clear_socket(ktp_socket *ksock)
{
    ksock->ip = 0;
    ksock->is_allocated = 0;
    ksock->pid = 0;
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

int increase_window(struct sld_wnd *wnd)
{
    wnd->back = (wnd->back + 1) % MAX_SEQ_NUMBER;
    return wnd->back;
}

int decrease_window(struct sld_wnd *wnd)
{
    wnd->front = (wnd->front + 1) % MAX_SEQ_NUMBER;
    return wnd->front;
}

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

int set_flag(int __flag, struct sld_wnd *wnd)
{
    wnd->flags = wnd->flags | __flag;
    return wnd->flags;
}

ktp_sockid k_socket(int __domain, int __type, int __protocol)
{
    ktp_socket_store *socket_store = get_socket_store();
    // int sockfd;
    sem_t *mutex;

    // sem_wait(&(socket_store->mutex));

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        printf("Checking if socket %d is free\n", i);
        printf(" mutex: %s\n", socket_store->socks[i].mutex);
        mutex = sem_open(socket_store->socks[i].mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

        printf("Mutex successfully created\n");
        sem_wait(mutex);

        printf("Mutex acquired\n");

        if (socket_store->socks[i].is_allocated)
        {
            sem_post(mutex);
            sem_close(mutex);
            continue;
        }

        printf("trying to create a socket\n");

        clear_socket(&(socket_store->socks[i]));

        socket_store->socks[i].is_allocated = 1;
        socket_store->socks[i].pid = getpid();

        printf("successfully created socket\n");

        sem_post(mutex);
        sem_close(mutex);

        // sem_post(&(socket_store->mutex));
        return i;
    }

    errno = ENOSPACE;
    // sem_post(&(socket_store->mutex));
    return -1;
}

int k_bind(ktp_sockid sock, SOCK_ADDR __src_addr, socklen_t __src_addr_len, SOCK_ADDR __cli_addr, socklen_t __cli_addr_len)
{
    ktp_socket *ksock = get_socket(sock);
    sem_t *mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);
    int ret;

    printf("Trying kbind\n");
    printf("Mutex %s\n", ksock->mutex);

    if (ksock == NULL)
    {
        errno = INVALIDSOCKET;
        return -1;
    }

    printf("Waiting for mutex\n");

    sem_wait(mutex);

    printf("Got the mutex\n");

    if (ksock->is_allocated == 0)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = INVALIDSOCKET;
        return -1;
    }

    printf("The ksock is allocated.. Trying to bind\n");

    // if ((ret = bind(ksock->sockfd, __src_addr, __src_addr_len)) < 0)
    // {
    //     printf("COuld not bind\n");
    //     sem_post(mutex);
    //     sem_close(mutex);
    //     errno = NOTBINDABLE;
    //     return -1;
    // }

    struct sockaddr_in *t = (struct sockaddr_in *)__cli_addr;

    ksock->ip = t->sin_addr.s_addr;
    ksock->port = t->sin_port;

    t = (struct sockaddr_in *)__src_addr;

    ksock->s_ip = t->sin_addr.s_addr;
    ksock->s_port = t->sin_port;

    printf("Bidning done\n");

    sem_post(mutex);
    sem_close(mutex);
    return ret;
}

ssize_t k_sendto(ktp_sockid sock, void *buf, size_t n, int flags, SOCK_ADDR __addr, socklen_t __addr_len)
{
    ktp_socket *ksock = get_socket(sock);
    sem_t *mutex;
    int seqno;
    char *_buf;
    struct sockaddr_in *t = (struct sockaddr_in *)__addr;

    printf("trying to send\n");

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

    // printf("Trying to get mutex\n");

    mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    // printf("Waiting for mutex\n");

    sem_wait(mutex);

    // printf("Got mutex\n");

    if (ksock->is_allocated == 0)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = INVALIDSOCKET;
        perror("Socket is unallocated");
        return -1;
    }

    // printf("Checkign address\n");
    // printf("t_addr: %d, ip: %d, t_inport: %d, sockport: %d\n", t->sin_addr.s_addr, ksock->ip, t->sin_port, ksock->port);

    if (t->sin_addr.s_addr != ksock->ip || t->sin_port != ksock->port)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = ENOTBOUND;
        perror("Dst addr != bound address");
        return -1;
    }

    // printf("Checking left size\n");

    if (ksock->send_buf.size == MAX_MESSAGES)
    {
        sem_post(mutex);
        sem_close(mutex);
        errno = ENOSPACE;
        perror("Send buffer is full");
        return -1;
    }

    seqno = ksock->send_buf.ptr;

    printf("Copying message in buf %d\n", seqno);

    for (int i = 0; i < MESSAGE_SIZE; i++)
    {
        ksock->send_buf.s[seqno][i] = _buf[i];
    }
    ksock->send_buf.occ[seqno] = 1;

    ksock->send_buf.size++;
    ksock->send_buf.ptr = (seqno + 1) % MAX_MESSAGES;

    // printf("Next pointed %d\n", ksock->send_buf.ptr);

    sem_post(mutex);
    sem_close(mutex);

    return n;
}

ssize_t k_recvfrom(ktp_sockid sock, void *buf, size_t n, int flags, SOCK_ADDR __addr, socklen_t *__addr_len)
{
    ktp_socket *ksock = get_socket(sock);
    sem_t *mutex;
    int ret, seqno;
    char *_buf;
    struct sockaddr_in *t = (struct sockaddr_in *)__addr;

    printf("Trying to recieve\n");

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

    printf("Waiting for mutex\n");

    mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    sem_wait(mutex);

    printf("Got the mutex\n");

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
        printf("Recieve buff size : %d\n", ksock->recv_buf.size);
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

int k_isempty(ktp_sockid sock)
{
    ktp_socket *ksock = get_socket(sock);

    if (ksock == NULL)
    {
        return -1;
    }

    sem_t *mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    // sem_wait(&(socket_store->mutex));
    sem_wait(mutex);

    int isempty = (ksock->recv_buf.size == 0) && (ksock->send_buf.size == 0);

    printf("send buf size %d\n", ksock->send_buf.size);
    printf("recv buf size %d\n", ksock->recv_buf.size);

    sem_post(mutex);

    return isempty;
}

int k_close(ktp_sockid sock)
{
    ktp_socket_store *socket_store = get_socket_store();
    ktp_socket *ksock = get_socket(sock);

    if (ksock == NULL)
    {
        return 0;
    }

    sem_t *mutex = sem_open(ksock->mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);

    // sem_wait(&(socket_store->mutex));
    sem_wait(mutex);

    ksock->is_allocated = 0;
    ksock->pid = 0;

    sem_post(mutex);
    sem_close(mutex);
    // sem_post(&(ksock->mutex));
}
