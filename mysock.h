#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#define MAX_SEQUENCE_NUMBER 16
#define MAX_WINDOW_SIZE 5
#define MAX_BUFFER_SIZE 10
#define MSG_ID_SIZE 2
#define MAX_SOCKETS 25
#define SOCK_MTP 15
#define ACK_TYPE 'A'
#define DATA_TYPE 'D'
#define NOT_IMPLEMENTED 69
#define TYPE_SIZE sizeof(char)
#define MAX_FRAME_SIZE 1024
#define T 5
#define P(semid) semop(semid,&pop,1)
#define V(semid) semop(semid,&vop,1)
typedef struct
{
    short sequence_number;
    char type;
    char data[1024];
    struct sockaddr_in from_addr;
} recv_packet;

typedef struct
{
    struct timeval time;
    int sequence_number;
    char type;
    char data[1024];
    struct sockaddr_in to_addr;
} send_packet;

typedef struct
{
    int size;
    int front;
    int rear;
    send_packet window[MAX_WINDOW_SIZE];
    // send_packet  **window;
} send_window;

typedef struct
{
    int size;
    int front;
    int rear;
    recv_packet window[MAX_WINDOW_SIZE];
} recv_window;

typedef struct
{
    int size;
    int front;
    int rear;
    send_packet buffer[MAX_BUFFER_SIZE];
} send_buff;

typedef struct
{
    int size;
    int front;
    int rear;
    recv_packet buffer[MAX_BUFFER_SIZE];
} recv_buff;

typedef struct
{
    int is_free;
    pid_t pid;
    int sockfd;
    char ip[20];
    int port;
    int flag_nospace;
    int last_ack;
    int last_seq;
    send_window swnd;
    send_buff sbuff;
    recv_window rwnd;
    recv_buff rbuff;
} shared_memory;
                
typedef struct
{
    int sockfd;
    char ip[20];
    int port;
    int error_no;
} sock_info;
// Functions available to application

int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port);
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);
int dropMessage(float p);



void cur_init()
{
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
    int key_SM = ftok(".", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);
    int key_sockinfo = ftok(".", 'B');
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = (sock_info *)shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    int key_sem3 = ftok(".", 'E'); // semaphore for shared memory SM
    int sem_SM = semget((key_t)key_sem3, 1, IPC_CREAT | 0666);
    // initialize the semaphores
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    semctl(sem_SM, 0, SETVAL, 1);
    struct sembuf pop, vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    P(sem_SM);
    sockinfo->sockfd = 0;
    sockinfo->port = 0;
    sockinfo->ip[0] = '\0';
    sockinfo->error_no = 0;
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].sockfd = -1;
        SM[i].port = -1;
        memset(SM[i].ip, 0, sizeof(SM[i].ip));
        SM[i].is_free = 1;
        SM[i].pid = -1;
        SM[i].flag_nospace = 0;
        SM[i].last_ack = 0;
        SM[i].sbuff.front = 0;
        SM[i].sbuff.rear = 0;
        SM[i].sbuff.size = 0;
        SM[i].rbuff.front = 0;
        SM[i].rbuff.rear = 0;
        SM[i].rbuff.size = 0;
    }

    recv_packet rpkt ;
    rpkt.sequence_number = 0;
    rpkt.type = DATA_TYPE;
    rpkt.from_addr.sin_family = AF_INET;
    rpkt.from_addr.sin_port = htons(8080);
    rpkt.from_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    strcpy(rpkt.data, "Hello");
    SM[0].rbuff.buffer[0] = rpkt;
    V(sem_SM);
    shmdt(SM);
    shmdt(sockinfo);
    // return 0;
}

void print(shared_memory *SM)
{
    printf("is_free-> %d  pid-> %d  sockfd-> %d  ip-> %s  port-> %d  flag_nospace=%d  last_ack-> %d\n last_seq-> %d", SM->is_free, SM->pid, SM->sockfd, SM->ip, SM->port, SM->flag_nospace, SM->last_ack,SM->last_seq);
}

int m_socket(int domain, int type, int protocol)
{
    // create a UDP socket
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
    key_t key_SM = ftok(".", 'A');
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(shared_memory), 0666);
    int key_sockinfo = ftok(".", 'B');
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info),0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    if (sockinfo == (void *)-1)
    {
        perror("[-] Error in mapping sockinfo");
        exit(EXIT_FAILURE);
    }
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1,0666);
    int sem2 = semget((key_t)key_sem2, 1,0666);
    int key_sem3 = ftok(".", 'E'); // semaphore for shared memory SM
    int sem_SM = semget((key_t)key_sem3, 1,0666);

    struct sembuf pop;
    struct sembuf vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    
    if (type != SOCK_MTP)
    {
        perror("Socket type not supported");
        exit(EXIT_FAILURE);
    }
    printf("Creating socket test\n");
 

    int i;
    P(sem_SM);
    printf("Creating socket test 2\n");
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].is_free)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        errno = ENOBUFS;
        printf("No space available\n");
        V(sem_SM);
        return -1;
    }
    else
    {
        printf(" i is %d\n", i);
        SM[i].is_free = 0;
        printf("sockinfo->sockfd is %d\n", sockinfo->sockfd);
        V(sem1);
        printf(" Signal sent to sem1\n");
        P(sem2);
        printf(" Signal received from sem2\n");
        if (sockinfo->sockfd != -1)
        {
            // memset(sockinfo, 0, sizeof(sockinfo));
            SM[i].sockfd = sockinfo->sockfd;
            printf("sockfd is %d\n", sockinfo->sockfd);
            // reset the sockinfo
            sockinfo->sockfd = 0;
            sockinfo->port = 0;
            sockinfo->ip[0] = '\0';
            sockinfo->error_no = 0;
            V(sem_SM);
            return i;
        }
        else
        {
            // memset(sockinfo, 0, sizeof(sockinfo));
            printf("sockinfo->sockfd should be  %d\n", sockinfo->sockfd);
            errno = ENOBUFS;
            // reset the sockinfo
            sockinfo->sockfd = 0;
            sockinfo->port = 0;
            sockinfo->ip[0] = '\0';
            sockinfo->error_no = 0;
            V(sem_SM);
            exit(EXIT_FAILURE);
        }
    }
    V(sem_SM);
    // shmdt(SM);
    // shmdt(sockinfo);
    return i;
}

int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port)
{
    key_t key_SM = ftok(".", 'A');
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(shared_memory),0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int key_sockinfo = ftok(".", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info),0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1,0666);
    int sem2 = semget((key_t)key_sem2, 1,0666);
    int key_sem3 = ftok(".", 'E'); // semaphore for shared memory SM
    int sem_SM = semget((key_t)key_sem3, 1,0666);

    struct sembuf pop;
    struct sembuf vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    int i;
    // printf("sockfd is %d ",sockfd);
    P(sem_SM);
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        // printf("i::::%d\n", i);
        // print(&SM[i]);
        if (i == sockfd)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        printf("HI\n ");
        // exit(EXIT_FAILURE);
        return -1;
    }
    // printf("%d\n",sockinfo->sockfd);
    sockinfo->sockfd = SM[i].sockfd;
    strcpy(sockinfo->ip, source_ip);
    sockinfo->port = source_port;
    // update the SM[i] table
    strcpy(SM[i].ip, dest_ip);
    SM[i].port = dest_port;
    // printf("%d==>\n", i);
    print(&SM[i]);
    V(sem1);
    P(sem2);
    if (sockinfo->sockfd != -1)
    {
        // printf("sockinfo->sockfd is %d\n", sockinfo->sockfd);
        // memset(sockinfo, 0, sizeof(sockinfo));
        // reset the sockinfo
        sockinfo->sockfd = 0;
        sockinfo->port = 0;
        sockinfo->ip[0] = '\0';
        sockinfo->error_no = 0;
        printf("Returning from bind\n");
        V(sem_SM);
        return 0;
    }
    else
    {
        // memset(sockinfo, 0, sizeof(sockinfo));
        // reset the sockinfo
        printf("sockinfo->sockfd is %d\n", sockinfo->sockfd);
        sockinfo->sockfd = 0;
        sockinfo->port = 0;
        sockinfo->ip[0] = '\0';
        sockinfo->error_no = 0;
        printf("Returning from bind 2\n");
        errno = ENOBUFS;
        V(sem_SM);
        exit(EXIT_FAILURE);
    }
    V(sem_SM);
    // shmdt(SM);
    // shmdt(sockinfo);
    printf("Returning from bind\n");
    return 0;
}

int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    key_t key_SM = ftok(".", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int key_sockinfo = ftok(".", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    int key_sem3 = ftok(".", 'E'); // semaphore for shared memory SM
    int sem_SM = semget((key_t)key_sem3, 1, IPC_CREAT | 0666);

    struct sembuf pop;
    struct sembuf vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (i == sockfd)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        return -1;
        // exit(EXIT_FAILURE);
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)dest_addr;
    P(sem_SM);
    char ip[20];
    strcpy(ip, inet_ntoa(addr->sin_addr));
    int port = ntohs(addr->sin_port);

    if ((strcmp(SM[i].ip, ip) == 0 && SM[i].port == port))
    {
        printf("IP and port are same\n");
        // write message to the sender side buffer
        send_packet spkt ;
        char new_buf[MAX_FRAME_SIZE];
        memset(new_buf,0,sizeof(new_buf));
        new_buf[0] = DATA_TYPE;
        // new_buf[1]=0;
        // get the last sequence number
        int lastSeq = SM[i].last_seq;
        // update the sequence number
        spkt.sequence_number = lastSeq;
        SM[i].last_seq = (SM[i].last_seq + 1) % MAX_SEQUENCE_NUMBER;
        // Convert and copy the sequence number to new_buf
        char seq_number[MSG_ID_SIZE]; // Assuming MSG_ID_SIZE is the size of short
        printf("spkt->sequence_number is %d\n", (int)spkt.sequence_number);
        sprintf(seq_number, "%d", (int)spkt.sequence_number);
        printf("strlen(new_buf) is %d and newbuf is currently %s\n", strlen(new_buf), new_buf);
        strcat(new_buf, seq_number);

        // Copy the message data (buf) to new_buf using strcat
        strcat(new_buf, buf);

        // Copy new_buf to spkt->data using strcpy
        strcpy(spkt.data, new_buf);
        printf("spkt->data is %s and \n", spkt.data); 
        // update all other fields of spkt
        spkt.type = DATA_TYPE;
        // get time of day and store in spkt->time
        gettimeofday(&spkt.time, NULL);
        // store the destination address in spkt->to_addr
        spkt.to_addr = *addr;
        SM[i].sbuff.buffer[SM[i].sbuff.rear] = spkt;
        // printf("i => %d\n", i);
        printf("SM[i].sbuff.buffer[SM[i].sbuff.rear]->data is %s\n", SM[i].sbuff.buffer[SM[i].sbuff.rear].data);
        SM[i].sbuff.rear = (SM[i].sbuff.rear + 1) % MAX_BUFFER_SIZE;
        SM[i].sbuff.size++;
        for(int j=0;j<MAX_BUFFER_SIZE;j++)
        {
            printf("SM[i].sbuff.buffer[j]->data is %s\n", SM[i].sbuff.buffer[j].data);
        }
        // update the send window 
        // allocate memory for the window

        SM[i].swnd.window[SM[i].swnd.rear] = spkt;
        // printf("SM[i].swnd.window is %d\n", SM[i].swnd.window->data);
        printf("SM[i].swnd.window[SM[i].swnd.rear]->data is %s\n", SM[i].swnd.window[SM[i].swnd.rear].data);
        SM[i].swnd.rear = (SM[i].swnd.rear + 1) % MAX_WINDOW_SIZE;
        SM[i].swnd.size++;
        V(sem_SM);
    }
    else
    {
        // set global ERROR to ENOBUFS
        errno = ENOBUFS;
        V(sem_SM);
        return -1;
    }

    return len;
}

int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    key_t key_SM = ftok(".", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory),0666);
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    printf(" key_SM is %d\n", key_SM);
    int key_sockinfo = ftok(".", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info),0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    int key_sem3 = ftok(".", 'E'); // semaphore for shared memory SM
    printf("key_sem3 is %d\n", key_sem3);
    int sem_SM = semget((key_t)key_sem3, 1,0666);
    int key_sem4 = ftok(".", 'F');
    int sem_SM2 = semget((key_t)key_sem4, 1,0666);


    struct sembuf pop;
    struct sembuf vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (i == sockfd)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        exit(EXIT_FAILURE);
    }
    P(sem_SM);
    if (SM[i].rbuff.front == SM[i].rbuff.rear)
    {
        errno = ENOMSG;
        // exit(EXIT_FAILURE);
    }
    P(sem_SM2);
    printf("SM[i].rbuff.front is %d\n", SM[i].rbuff.front);
    recv_packet rpkt1 = SM[i].rbuff.buffer[SM[i].rbuff.front]; // first message in the buffer
    printf("rpkt->data is %s\n", rpkt1.data);
    printf(" SM[i].rbuff.size is %d\n", SM[i].rbuff.size);
    for(int j=0;j<MAX_BUFFER_SIZE;j++)
    {
        printf("SM[i].rbuff.buffer[j]->data is %s\n", SM[i].rbuff.buffer[j].data);
    }
    // SM[i].rbuff.buffer[SM[i].rbuff.front] = NULL;
    SM[i].rbuff.front = (SM[i].rbuff.front + 1) % MAX_BUFFER_SIZE;
    strcpy((char *)buf, rpkt1.data);
    V(sem_SM2);
    V(sem_SM);
    // return no of bytes received
    return len;
}

int m_close(int sockfd)
{
    int key_SM = ftok(".", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int key_sockinfo = ftok(".", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);

    // struct sembuf pop;
    // struct sembuf vop;
    // pop.sem_num = 0;
    // pop.sem_op = -1;
    // pop.sem_flg = 0;
    // vop.sem_num = 0;
    // vop.sem_op = 1;
    // vop.sem_flg = 0;

    //     int i;
    //     for (i = 0; i < MAX_SOCKETS; i++)
    //     {
    //         if (SM[i].sockfd == sockfd)
    //             break;
    //     }

    //     if (i == MAX_SOCKETS)
    //     {
    //         perror("[-] Address not found");
    //         exit(EXIT_FAILURE);
    //     }

    //     if (SM[i].rbuff.front == SM[i].rbuff.rear)
    //     {
    //         errno = ENOMSG;
    //         exit(EXIT_FAILURE);
    //     }
    //     SM[i].is_free = 1;
    //     return close(sockfd);
    return NOT_IMPLEMENTED;
}

int dropMessage(float p)
{
    return NOT_IMPLEMENTED;
}

// int main()
// {
//     // testing m_socket()
//     // cur_init();
//     int ret1 = m_socket(AF_INET, SOCK_MTP, 0); // working
//     printf("ret from m_socket => %d\n", ret1);
//     // run bind
//     int ret = m_bind(ret1, "127.0.0.1", 8080, "127.0.0.1", 8080); // working
//     printf("%d\n", ret);
//     // run sendto
//     char *buf = "Hello";
//     struct sockaddr_in dest_addr;
//     dest_addr.sin_family = AF_INET;
//     dest_addr.sin_port = htons(8080);
//     dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//     ret = m_sendto(ret1, buf, strlen(buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
//     printf("ret sendto => %d\n", ret);
//     // run recvfrom
//     char *buf2 = (char *)malloc(1024);
//     struct sockaddr_in src_addr;
//     src_addr.sin_family = AF_INET;
//     src_addr.sin_port = htons(8080);
//     src_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//     socklen_t addrlen;
//     addrlen = sizeof(src_addr);
//     ret = m_recvfrom(ret1, buf2, 1024, 0, (struct sockaddr *)&src_addr, &addrlen);
//     // printf("ret recv_from=> %d \n", ret);
//     // printf("buf2 => %s\n", buf2);

//     exit(EXIT_SUCCESS);
// }