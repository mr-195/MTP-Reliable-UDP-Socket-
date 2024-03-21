#include "mysock.h"

int cur_init()
{
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

    struct sembuf pop;
    struct sembuf vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].is_free = 1;
    }
    return 0;
}

// typedef struct
// {
//     int is_free;
//     pid_t pid;
//     int sockfd;
//     char ip[20];
//     int port;
//     int flag_nospace;
//     int last_ack;
//     send_window swnd;
//     send_buff sbuff;
//     recv_window rwnd;
//     recv_buff rbuff;
// } shared_memory;

void print(shared_memory *SM)
{
    printf("is_free-> %d  pid-> %d  sockfd-> %d  ip-> %s  port-> %d  flag_nospace=%d  last_ack-> %d\n", SM->is_free, SM->pid, SM->sockfd, SM->port, SM->flag_nospace, SM->last_ack);
}

int m_socket(int domain, int type, int protocol)
{
    // create a UDP socket
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);

    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

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
    int sockfd = socket(domain, SOCK_DGRAM, protocol);

    if (sockfd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        print(&SM[i]);
        if (SM[i].is_free)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        errno = ENOBUFS;
        return -1;
    }
    else
    {
        // V(sem1);
        // P(sem2);
        if (sockinfo->sockfd != -1)
        {
            memset(sockinfo, 0, sizeof(sockinfo));
            SM[i].sockfd = sockfd;
            return sockfd;
        }
        else
        {
            memset(sockinfo, 0, sizeof(sockinfo));
            errno = ENOBUFS;
            exit(EXIT_FAILURE);
        }
    }

    return i;
}

int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port)
{

    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

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
        if (SM[i].sockfd == sockfd)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        exit(EXIT_FAILURE);
    }
    sockinfo->sockfd = sockfd;
    strcpy(sockinfo->ip, source_ip);
    V(sem1);
    P(sem2);
    if (sockinfo->sockfd != -1)
    {
        memset(sockinfo, sizeof(sockinfo), 0);
        exit(EXIT_SUCCESS);
    }
    else
    {
        memset(sockinfo, sizeof(sockinfo), 0);
        errno = ENOBUFS;
        exit(EXIT_FAILURE);
    }
}

int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

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
        if (SM[i].sockfd == sockfd)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)dest_addr;

    char ip[20];
    strcpy(ip, inet_ntoa(addr->sin_addr));
    int port = ntohs(addr->sin_port);
    if (strcmp(SM[i].ip, ip) && SM[i].port == port)
    {
        // write message to the sender side buffer
        send_packet *spkt = (send_packet *)malloc(sizeof(send_packet));
        char new_buf[MAX_FRAME_SIZE];
        int offset = 0;

        // Copy the DATA_TYPE to new_buf
        char temp[2];
        temp[1] = 0;
        temp[0] = DATA_TYPE;
        strcpy(new_buf, temp);
        offset += strlen(temp); // Update offset //size issue could be there

        // Convert and copy the sequence number to new_buf
        short seq_number = htons(spkt->sequence_number); // Assuming sequence_number is short
        memcpy(new_buf + offset, &seq_number, MSG_ID_SIZE);
        offset += MSG_ID_SIZE; // Update offset

        // Copy the message data (buf) to new_buf
        memcpy(new_buf + offset, buf, len);
        offset += len; // Update offset

        // Copy new_buf to spkt->message.data
        memcpy(spkt->data, new_buf, offset);

        spkt->to_addr = *addr;
        SM[i].sbuff.buffer[SM[i].sbuff.rear] = spkt;
        SM[i].sbuff.rear = (SM[i].sbuff.rear + 1) % SM[i].sbuff.size;
    }
    else
    {
        // set global ERROR to ENOBUFS
        errno = ENOBUFS;
        return -1;
    }

    return len;
}

int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

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
        if (SM[i].sockfd == sockfd)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        exit(EXIT_FAILURE);
    }

    if (SM[i].rbuff.front == SM[i].rbuff.rear)
    {
        errno = ENOMSG;
        exit(EXIT_FAILURE);
    }

    recv_packet *rpkt = SM[i].rbuff.buffer[SM[i].rbuff.front];
    rpkt = SM[i].rbuff.buffer[SM[i].rbuff.front];
    SM[i].rbuff.buffer[SM[i].rbuff.front] = NULL;
    SM[i].rbuff.front = (SM[i].rbuff.front + 1) % SM[i].rbuff.size;
}

int m_close(int sockfd)
{
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

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
        if (SM[i].sockfd == sockfd)
            break;
    }

    if (i == MAX_SOCKETS)
    {
        perror("[-] Address not found");
        exit(EXIT_FAILURE);
    }

    if (SM[i].rbuff.front == SM[i].rbuff.rear)
    {
        errno = ENOMSG;
        exit(EXIT_FAILURE);
    }
    SM[i].is_free = 1;
    return close(sockfd);
}

int dropMessage(float p)
{
    return NOT_IMPLEMENTED;
}

int main()
{
    // testing m_socket()
    cur_init();
    int ret = m_socket(AF_INET, SOCK_MTP, 0);
    printf("%d\n", ret);

    exit(EXIT_SUCCESS);
}