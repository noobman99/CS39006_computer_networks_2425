// =====================================
// Assignment 5 Submission
// Name: Parth Shashin Patil
// Roll number: 22CS30041
// =====================================

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define MAXLINE 100
#define MAX_TASKLEN 20
#define MAX_TASKS 50
#define MAX_WAIT_TIME 60

struct queue
{
    int front;
    int back;
    char q[MAX_TASKS][MAX_TASKLEN];
};

int P(int semid)
{
    struct sembuf p_op;
    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = 0;
    semop(semid, &p_op, 1);
}

int V(int semid)
{
    struct sembuf v_op;
    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = 0;
    semop(semid, &v_op, 1);
}

struct queue *SM;
int mutex, shmid;

int read_tasks()
{
    FILE *fp;
    char task[MAX_TASKLEN], c;

    fp = fopen("config.txt", "r");

    if (fp == NULL)
    {
        printf("Error opening file\n");
        return -1;
    }

    int i = 0, j = 0;
    while ((c = fgetc(fp)) != EOF)
    {
        if (j == MAX_TASKS)
        {
            break;
        }

        if (c == '\n')
        {
            task[i] = '\0';
            strcpy(SM->q[j], task);
            j++;
            i = 0;
        }
        else
        {
            task[i] = c;
            i++;
        }
    }
    if (i > 0 && j < MAX_TASKS)
    {
        task[i] = '\0';
        strcpy(SM->q[j], task);
        j++;
    }

    SM->back = j;
    return 1;
}

char *get_task()
{
    P(mutex);

    if (SM->front == SM->back)
    {
        V(mutex);
        return NULL;
    }

    char *task = SM->q[SM->front];
    SM->front = (SM->front + 1) % MAX_TASKS;
    V(mutex);

    char *buf = (char *)malloc(MAX_TASKLEN);
    strcpy(buf, task);

    return buf;
}

void push_front(char *task)
{
    P(mutex);

    SM->front = (SM->front - 1 + MAX_TASKS) % MAX_TASKS;
    strcpy(SM->q[SM->front], task);

    V(mutex);
}

int check_request_type(char *buffer)
{
    char get_task[] = "GET_TASK";
    char result[] = "RESULT";
    char close[] = "CLOSE";

    int i;

    for (i = 0; i < strlen(get_task); i++)
    {
        if (buffer[i] != get_task[i])
        {
            break;
        }
    }

    if (i == strlen(get_task))
    {
        return 1;
    }

    for (i = 0; i < strlen(result); i++)
    {
        if (buffer[i] != result[i])
        {
            break;
        }
    }

    if (i == strlen(result))
    {
        return 2;
    }

    for (i = 0; i < strlen(close); i++)
    {
        if (buffer[i] != close[i])
        {
            break;
        }
    }

    if (i == strlen(close))
    {
        return 3;
    }

    return 0;
}

int get_result(char *buffer)
{
    int result = 0;

    sscanf(buffer, "RESULT %d", &result);

    return result;
}

int handle_client(int clisockfd)
{
    int n, updtime = 1;
    char buffer[MAXLINE];
    char *task = NULL;
    time_t last_opt_time = time(NULL);

    printf("Client connected\n");

    while (1)
    {
        memset(buffer, 0, MAXLINE);
        n = recv(clisockfd, buffer, MAXLINE, MSG_WAITALL);

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            if (time(NULL) - last_opt_time > MAX_WAIT_TIME)
            {
                if (task != NULL)
                {
                    push_front(task);
                    free(task);
                }
                close(clisockfd);
                exit(0);
            }
            usleep(10000);
            continue;
        }

        if (n < 0)
        {
            printf("Socket Error\n");
            if (task != NULL)
            {
                push_front(task);
                free(task);
            }
            close(clisockfd);
            exit(0);
        }

        updtime = 1;

        if (n == 0)
        {
            if (task != NULL)
            {
                push_front(task);
                free(task);
            }
            close(clisockfd);
            break;
        }

        switch (check_request_type(buffer))
        {
        case 1:
        {
            memset(buffer, 0, MAXLINE);
            if (task == NULL)
            {
                task = get_task();
            }
            else
            {
                updtime = 0;
                break;
            }
            if (task == NULL)
            {
                sprintf(buffer, "NO_TASK");
            }
            else
            {
                sprintf(buffer, "TASK: %s", task);
            }
            printf("Sending: %s\n", buffer);
            while (send(clisockfd, buffer, MAXLINE, 0) < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    usleep(10000);
                    continue;
                }

                printf("Socket Error\n");
                push_front(task);
                free(task);
                close(clisockfd);
                exit(0);
            }
            break;
        }
        case 2:
        {
            if (task == NULL)
            {
                updtime = 0;
                printf("No task to get result\n");
                break;
            }
            int result = get_result(buffer);
            printf("Result for task (%s): %d\n", task, result);
            free(task);
            task = NULL;
            break;
        }
        case 3:
        {
            if (task != NULL)
            {
                push_front(task);
                free(task);
            }
            close(clisockfd);
            exit(0);
            break;
        }
        default:
            printf("Invalid command\n");
            break;
        }

        if (updtime)
        {
            last_opt_time = time(NULL);
        }
    }

    exit(0);
}

void handle_sigterm(int sig)
{
    shmctl(shmid, IPC_RMID, NULL);
    semctl(mutex, 0, IPC_RMID);
    exit(0);
}

int main()
{
    struct sockaddr_in servaddr, cliaddr;
    int sockfd, clisockfd, len, n;

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failed...\n");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    // Bind the socket
    if ((bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("Socket bind failed...\n");
        exit(0);
    }

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // Listen on the socket
    if ((listen(sockfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }

    // initialize shared memory
    shmid = shmget(IPC_PRIVATE, sizeof(struct queue), 0666 | IPC_CREAT);
    SM = (struct queue *)shmat(shmid, 0, 0);
    SM->front = 0;
    SM->back = 0;
    memset(SM->q, 0, sizeof(SM->q));

    // initialize semaphore
    mutex = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    semctl(mutex, 0, SETVAL, 1);

    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);
    signal(SIGQUIT, handle_sigterm);
    signal(SIGKILL, handle_sigterm);

    if (read_tasks() < 0)
    {
        shmctl(shmid, IPC_RMID, NULL);
        semctl(mutex, 0, IPC_RMID);
        exit(0);
    }

    while (1)
    {
        len = sizeof(cliaddr);
        clisockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);

        // check for zombie processes

        // check for non-blocking return
        if (clisockfd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            continue;
        }

        if (clisockfd < 0)
        {
            printf("Server acccept failed, %d...\n", errno);
            perror("Error");
            break;
        }

        while (waitpid(-1, NULL, WNOHANG) > 0)
        {
            continue;
        }

        fcntl(clisockfd, F_SETFL, O_NONBLOCK);

        if (!fork())
        {
            close(sockfd);
            handle_client(clisockfd);
        }
        else
        {
            close(clisockfd);
        }
    }

    shmctl(shmid, IPC_RMID, NULL);
    semctl(mutex, 0, IPC_RMID);
}
