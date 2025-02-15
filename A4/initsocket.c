#include "ksocket.h"
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>

// global variables
extern int errno;
sem_t *mutex[MAX_SOCKETS];

int mmax(int x, int y)
{
    return x > y ? x : y;
}

struct timeout_eval
{
    time_t t;
    int front;
    int back;
};

ktp_socket_store *get_socket_store()
{
    key_t shmkey = ftok(FKEY, KTP_PROJECT);
    int shmid = shmget(shmkey, sizeof(ktp_socket_store), 0666 | IPC_CREAT);

    if (shmid < 0)
    {
        errno = NOSOCKETRUNNING;
        perror("No shared memory was intialized");
        exit(1);
    }

    ktp_socket_store *socket_store = shmat(shmid, NULL, 0);

    return socket_store;
}

ssize_t create_packet(char *packet, int m, char *buf, int n, int is_ack, int seq_number, int ack_number, int recv_size)
{
    if (m < n + HEADER_SIZE)
    {
        return -1;
    }

    // headers -
    packet[0] = (u_int8_t)is_ack; // is ack
    packet[1] = (u_int8_t)seq_number;
    packet[2] = (u_int8_t)ack_number;
    packet[3] = (u_int8_t)recv_size;

    int i = 0, j = HEADER_SIZE;
    while (i < n)
    {
        packet[j] = buf[i];
        j++;
        i++;
    }

    return n + HEADER_SIZE;
}

ssize_t decode_packet(char *packet, int m, char *buf, int n, int *is_ack, int *seq_number, int *ack_number, int *recv_size)
{
    if (n < m - HEADER_SIZE)
    {
        return -1;
    }

    *is_ack = (int)packet[0];
    *seq_number = (int)packet[1];
    *ack_number = (int)packet[2];
    *recv_size = (int)packet[3];

    int i = 0, j = HEADER_SIZE;

    while (j < m)
    {
        buf[i] = packet[j];
        j++;
        i++;
    }

    return m - HEADER_SIZE;
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

void handlesigint(int signo)
{
    printf("KILLING THE PROCESS\n");

    key_t shmkey = ftok(FKEY, KTP_PROJECT);
    int shmid = shmget(shmkey, sizeof(ktp_socket_store), 0666 | IPC_CREAT);

    if (shmid < 0)
    {
        errno = NOSOCKETRUNNING;
        perror("No shared memory initialized");
        exit(1);
    }

    ktp_socket_store *socket_store = shmat(shmid, NULL, 0);

    if (socket_store == (void *)-1)
    {
        perror("Invalid SHMAT");
        exit(1);
    }

    sem_t *mutex;

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        if (socket_store->socks[i].is_allocated)
        {
            kill(socket_store->socks[i].pid, SIGINT);
            close(socket_store->socks[i].sockfd);
        }
        sem_unlink(socket_store->socks[i].mutex);
    }

    shmdt(socket_store);
    shmctl(shmid, IPC_RMID, NULL);
    exit(0);
}

int R()
{
    ktp_socket_store *store = get_socket_store();
    fd_set fds;
    ktp_socket *ksock;
    int nfds, nread, alen, nwrite, swdsize, frnt;
    int is_ack, ack_num, seq_num, recv_size, datalen, buff_idx;
    char buf[MESSAGE_SIZE], packet[MESSAGE_SIZE + HEADER_SIZE];
    struct sockaddr_in addr;
    struct timeval tv;
    // sem_t *mutex[MAX_SOCKETS];

    // for (int i = 0; i < MAX_SOCKETS; i++)
    // {
    //     mutex[i] = sem_open(store->socks[i].mutex, SEM_FLAGS, SEM_INITVAL, SEM_INITVAL);
    // }

    while (1)
    {
        FD_ZERO(&fds);
        nfds = 0;

        // sem_wait(&(store->mutex));

        // initialize the file descriptor set
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            ksock = &store->socks[i];
            sem_wait(mutex[i]);
            if (ksock->is_allocated)
            {
                FD_SET(ksock->sockfd, &fds);
                printf("R_Socket %d: Adding %d to recieve fds\n", i, ksock->sockfd);
                nfds = mmax(nfds, ksock->sockfd);
            }
            sem_post(mutex[i]);
        }

        // sem_post(&(store->mutex));

        tv.tv_sec = 3;
        tv.tv_usec = 0;

        printf("R_ : nfds = %d\n", nfds + 1);

        // wait for select
        if (select(nfds + 1, &fds, NULL, NULL, &tv) < 0)
        {
            perror("Select errored out");
            switch (errno)
            {
            case EBADF:
                printf("Error: EBADF - Invalid file descriptor.\n");
                break;
            case EINTR:
                printf("Error: EINTR - System call interrupted by a signal.\n");
                break;
            case EINVAL:
                printf("Error: EINVAL - Invalid argument (nfds exceeds limit or invalid timeout).\n");
                break;
            case ENOMEM:
                printf("Error: ENOMEM - Unable to allocate memory.\n");
                break;
            default:
                printf("Error: Unknown error (%d) - %s\n", errno, strerror(errno));
            }
            exit(1);
        }

        // sem_wait(&(store->mutex));

        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            ksock = &store->socks[i];
            if (!FD_ISSET(ksock->sockfd, &fds))
                continue;

            printf("R_Socket %d: Recieved a message \n", i);

            memset(&addr, 0, sizeof(addr));
            alen = sizeof(addr);

            sem_wait(mutex[i]);
            if (ksock->is_allocated)
            {
                // read message from socket
                nread = recvfrom(ksock->sockfd, packet, (MESSAGE_SIZE + HEADER_SIZE), 0, (struct sockaddr *)&addr, &alen);

                // check if they are from bound address and port
                if (addr.sin_addr.s_addr == ksock->ip && addr.sin_port == ksock->port)
                {
                    printf("R_Socket %d: Recieved a valid message yay\n", i);

                    if (nread == 0)
                    {
                        ksock->is_allocated = 0;
                        ksock->pid = 0;
                        printf("R_Socket %d: They brokeup :( \n", i);
                    }
                    else
                    {
                        datalen = decode_packet(packet, nread, buf, MESSAGE_SIZE, &is_ack, &seq_num, &ack_num, &recv_size);
                        if (is_ack)
                        {
                            printf("R_Socket %d: It is an ACK for %d\n", i, ack_num);

                            // if the reciever packet is ACKNOWLEDGEMENT
                            if (ack_num == ksock->swnd.ack_num)
                            {
                                // duplicate ACK, only reset maximum size possible for send buffer
                                ksock->swnd.max_size = recv_size;
                            }
                            else
                            {
                                // for normal ACK
                                ksock->swnd.ack_num = ack_num;

                                if (in_window(ack_num, &ksock->swnd))
                                {
                                    // decrease size till ACKed packet
                                    frnt = ksock->swnd.front;
                                    while (in_window(frnt, &ksock->swnd) && frnt != ack_num)
                                    {
                                        // empty the buffer at given position
                                        memset(ksock->send_buf.s[frnt % MAX_MESSAGES], 0, MESSAGE_SIZE);
                                        ksock->send_buf.occ[frnt % MAX_MESSAGES] = 0;
                                        ksock->send_buf.size--;
                                        frnt = decrease_window(&ksock->swnd);
                                    }

                                    memset(ksock->send_buf.s[frnt % MAX_MESSAGES], 0, MESSAGE_SIZE);
                                    ksock->send_buf.occ[frnt % MAX_MESSAGES] = 0;
                                    ksock->send_buf.size--;
                                    decrease_window(&ksock->swnd);

                                    ksock->swnd.max_size = recv_size;
                                }
                            }
                        }
                        else
                        {
                            // if the recieved packet is data
                            printf("R_Socket %d: It is a message with seqnum %d\n", i, seq_num);

                            // check if in recieving window
                            if (in_window(seq_num, &ksock->rwnd))
                            {
                                // check if already in buffer
                                buff_idx = seq_num % 10;
                                if (ksock->recv_buf.occ[buff_idx] == 0)
                                {
                                    // copy into buffer
                                    printf("R_Socket %d: Writing into buffer with idx %d\n", i, buff_idx);

                                    ksock->recv_buf.occ[buff_idx] = 1;
                                    for (int i = 0; i < datalen; i++)
                                    {
                                        ksock->recv_buf.s[buff_idx][i] = buf[i];
                                    }
                                    ksock->recv_buf.size++;

                                    printf("R_Socket %d: recv buff size: %d\n", i, ksock->recv_buf.size);

                                    // check if in order message
                                    if (ksock->rwnd.front == seq_num)
                                    {
                                        // find the last in-order message
                                        ack_num = seq_num;
                                        frnt = seq_num;
                                        while (ksock->recv_buf.occ[frnt % MAX_MESSAGES] != 0 && in_window(frnt, &ksock->rwnd))
                                        {
                                            ack_num = frnt;
                                            frnt = decrease_window(&ksock->rwnd);
                                        }

                                        // send ACK with the seq number
                                        memset(packet, 0, (MESSAGE_SIZE + HEADER_SIZE));
                                        recv_size = get_window_size(&ksock->rwnd);
                                        if (recv_size == 0)
                                        {
                                            // set NOSPACE flag
                                            set_flag(NOSPACE, &ksock->rwnd);
                                            // set last acknowledged seq number
                                            ksock->rwnd.ack_num = ack_num;
                                        }
                                        nwrite = create_packet(packet, (MESSAGE_SIZE + HEADER_SIZE), NULL, 0, 1, 0, ack_num, recv_size);
                                        sendto(ksock->sockfd, packet, nwrite, 0, (struct sockaddr *)&addr, alen);

                                        printf("R_Socket %d: Sending ACK for %d\n", i, ack_num);
                                        printf("R_Socket %d: rwnd front %d, back %d, size %d\n", i, ksock->rwnd.front, ksock->rwnd.back, recv_size);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            sem_post(mutex[i]);
        }

        // sem_post(&(store->mutex));
    }
}

void S()
{
    ktp_socket_store *store = get_socket_store();
    struct timeout_eval lst_time[MAX_SOCKETS][2];
    ktp_socket *ksock;
    time_t time_stamp;
    int rend, bnum, seq_num;
    int to_send; // boolean
    char packet[MESSAGE_SIZE + HEADER_SIZE];
    int pcksize, alen;
    struct sockaddr_in addr;
    // sem_t *mutex[MAX_SOCKETS];

    // for (int i = 0; i < MAX_SOCKETS; i++)
    // {
    //     mutex[i] = sem_open(store->socks[i].mutex, SEM_FLAGS, SEM_INITVAL, SEM_INITVAL);
    // }

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        lst_time[i][0].t = -1;
        lst_time[i][1].t = -1;
    }

    while (1)
    {
        sleep(MESSAGE_TIMEOUT / 2);
        time_stamp = time(NULL);

        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            ksock = &store->socks[i];
            to_send = 0;

            printf("S_Socket %d: Checking \n", i);

            sem_wait(mutex[i]);

            // printf("S_Socket %d: got mutex\n", i);

            if (ksock->is_allocated == 0)
            {
                // printf("S_Socket %d: is not allocated\n", i);
                sem_post(mutex[i]);
                continue;
            }

            // printf("S_Socket %d: socket is allocated\n", i);

            // check if all messages in first lst_time are ACKed

            // printf("S_Socked %d: now: %ld, t[0]: %ld, timeout = %d\n", i, time_stamp, lst_time[i][0].t, MESSAGE_TIMEOUT);

            printf("S_Socket %d: lst[0]: %ld\n", i, lst_time[i][0].t);

            if (lst_time[i][0].t != -1)
            {
                rend = (lst_time[i][0].back + MAX_SEQ_NUMBER - 1) % MAX_SEQ_NUMBER;

                if (in_window(rend, &ksock->swnd))
                {
                    if (time_stamp >= lst_time[i][0].t + MESSAGE_TIMEOUT)
                    {
                        printf("S_Socket %d: Having to resend the messages\n", i);

                        // NOT ACKed && time > T (if time != T, then second lst_time would be empty)
                        // Packets to be resent -- to_send = 1
                        to_send = 1;
                        lst_time[i][0].t = time_stamp;

                        if (lst_time[i][1].t != -1)
                        {
                            // second lst_time is not empty
                            // merge it into first time part
                            lst_time[i][1].t = -1;
                            lst_time[i][0].back = lst_time[i][1].back;
                        }
                    }
                }
                else
                {
                    printf("S_Socket %d: Everything in lst 0 has been acked\n", i);
                    // Move second to first
                    // Set second to empty
                    lst_time[i][0] = lst_time[i][1];
                    lst_time[i][1].t = -1;
                }
            }

            // To check if there are any messages that can be added to sending window
            // add the messages into particular window
            rend = ksock->swnd.back;

            printf("S_Socket %d: curr back: %d, curr front: %d, max_size: %d\n", i, ksock->swnd.front, ksock->swnd.back, ksock->swnd.max_size);

            while (ksock->send_buf.occ[rend % MAX_MESSAGES] == 1 && get_window_size(&ksock->swnd) < ksock->swnd.max_size)
            {
                rend = increase_window(&ksock->swnd);
                to_send = 1;
            }

            printf("S_Socket %d: (UPDATED) curr back: %d, curr front: %d, max_size: %d\n", i, ksock->swnd.front, ksock->swnd.back, ksock->swnd.max_size);

            if (to_send)
            {
                // printf("S_Socket %d: Trying to send\n", i);

                // initialize the address
                addr.sin_addr.s_addr = ksock->ip;
                addr.sin_port = ksock->port;
                addr.sin_family = AF_INET;

                alen = sizeof(addr);

                // check which idx to send
                if (lst_time[i][0].t == -1 || lst_time[i][0].t == time_stamp)
                {
                    // send packets corresponding to lst_time 0
                    lst_time[i][0].t = time_stamp;
                    lst_time[i][0].front = ksock->swnd.front;
                    lst_time[i][0].back = ksock->swnd.back;

                    seq_num = lst_time[i][0].front;
                    while (seq_num != lst_time[i][0].back)
                    {
                        bnum = seq_num % MAX_MESSAGES;

                        // create packet
                        memset(packet, 0, MESSAGE_SIZE + HEADER_SIZE);
                        pcksize = create_packet(packet, (MESSAGE_SIZE + HEADER_SIZE), ksock->send_buf.s[bnum], MESSAGE_SIZE, 0, seq_num, 0, 0);

                        sendto(ksock->sockfd, packet, pcksize, 0, (struct sockaddr *)&addr, alen);

                        printf("S_Socket %d: Sent Message %d in lst 0\n", i, seq_num);

                        seq_num = (seq_num + 1) % MAX_SEQ_NUMBER;
                    }
                }
                else
                {
                    lst_time[i][1].t = time_stamp;
                    lst_time[i][1].front = lst_time[i][0].back;
                    lst_time[i][1].back = ksock->swnd.back;

                    seq_num = lst_time[i][1].front;
                    while (seq_num != lst_time[i][1].back)
                    {
                        bnum = seq_num % MAX_MESSAGES;

                        // create packet
                        memset(packet, 0, MESSAGE_SIZE + HEADER_SIZE);
                        pcksize = create_packet(packet, (MESSAGE_SIZE + HEADER_SIZE), ksock->send_buf.s[bnum], MESSAGE_SIZE, 0, seq_num, 0, 0);

                        sendto(ksock->sockfd, packet, pcksize, 0, (struct sockaddr *)&addr, alen);

                        printf("S_Socket %d: Sent Message %d in lst 1\n", i, seq_num);

                        seq_num = (seq_num + 1) % MAX_SEQ_NUMBER;
                    }
                }
            }

            sem_post(mutex[i]);
        }
    }
}

void initialize_socket(ktp_socket *ksock)
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

void G()
{
    ktp_socket_store *store = get_socket_store();
    ktp_socket *ksock;
    int val;

    while (1)
    {
        sleep(GC_TIMEOUT);

        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            ksock = &store->socks[i];
            if (ksock->is_allocated)
            {
                // check if process is dead
                if (kill(ksock->pid, 0) == -1)
                {
                    // check if mutex value is 0 -- to do
                    ksock->is_allocated = 0;

                    // reset socket
                    initialize_socket(ksock);
                }
            }
        }
    }
}

int main()
{

    // add signal handlers
    signal(SIGINT, handlesigint);
    signal(SIGSEGV, handlesigint);
    signal(SIGKILL, handlesigint);

    // initialize the shared memory
    key_t shmkey = ftok(FKEY, KTP_PROJECT);
    int shmid = shmget(shmkey, sizeof(ktp_socket_store), 0666 | IPC_CREAT | IPC_EXCL);
    char buf[100];

    if (shmid < 0)
    {
        errno = EXISTINGHANDLER;
        perror("There already exists a initsocket");
        exit(1);
    }

    // initialize the socket store
    ktp_socket_store *socket_store = shmat(shmid, NULL, 0);

    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        initialize_socket(&socket_store->socks[i]);
        socket_store->socks[i].sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        sprintf(socket_store->socks[i].mutex, "/ssem%d", i);
        mutex[i] = sem_open(socket_store->socks[i].mutex, SEM_FLAGS, SEM_PERMS, SEM_INITVAL);
    }

    if (fork() == 0)
    {
        // Run the R thread
        R();
        exit(0);
    }
    if (fork() == 0)
    {
        // Run the S thread
        S();
        exit(0);
    }
    if (fork() == 0)
    {
        // Run the garbage collector
        G();
        exit(0);
    }

    // Run the bind function
    struct sockaddr_in addr;
    ktp_socket *ksock;
    int alen;

    while (1)
    {
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            ksock = &socket_store->socks[i];

            sem_wait(mutex[i]);

            if (ksock->is_allocated == 0)
            {
                sem_post(mutex[i]);
                continue;
            }

            getsockname(ksock->sockfd, (struct sockaddr *)&addr, &alen);

            if (addr.sin_addr.s_addr != ksock->s_ip || addr.sin_port != ksock->s_port)
            {
                addr.sin_port = ksock->s_port;
                addr.sin_addr.s_addr = ksock->s_ip;

                alen = sizeof(addr);

                if (bind(ksock->sockfd, (struct sockaddr *)&addr, alen) < 0)
                {
                    perror("Binding error.");
                }
            }

            sem_post(mutex[i]);
        }
    }
}