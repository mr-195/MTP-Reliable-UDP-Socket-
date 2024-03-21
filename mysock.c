#include "mysock.h"
#include <sys/time.h>

void cur_init()
{
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);
    int key_sockinfo = ftok("/home/", 'B');
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = (sock_info *)shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    struct sembuf pop, vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
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
    }
    shmdt(SM);
    shmdt(sockinfo);
    // return 0;
}

void print(shared_memory *SM)
{
    printf("is_free-> %d  pid-> %d  sockfd-> %d  ip-> %s  port-> %d  flag_nospace=%d  last_ack-> %d\n", SM->is_free, SM->pid, SM->sockfd, SM->ip, SM->port, SM->flag_nospace, SM->last_ack);
}

int m_socket(int domain, int type, int protocol)
{
    // create a UDP socket
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), 0666 | IPC_CREAT);
    int key_sockinfo = ftok("/home/", 'B');
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), 0666 | IPC_CREAT);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    if (sockinfo == (void *)-1)
    {
        perror("[-] Error in mapping sockinfo");
        exit(EXIT_FAILURE);
    }
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);

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
    // update SM[i]

    int i;
    printf("Creating socket test 2\n");
    for (i = 0; i < MAX_SOCKETS; i++)
    {
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
        SM[i].sockfd = sockfd;
        // V(sem1);
        // P(sem2);
        if (sockinfo->sockfd != -1)
        {
            memset(&sockinfo, 0, sizeof(sockinfo));
            SM[i].sockfd = sockfd;
            // printf("sockfd is %d\n", sockfd);
            return i;
        }
        else
        {
            memset(&sockinfo, 0, sizeof(sockinfo));
            errno = ENOBUFS;
            exit(EXIT_FAILURE);
        }
    }
    shmdt(SM);
    shmdt(sockinfo);
    return i;
}

int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port)
{
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int key_sockinfo = ftok("/home/", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);

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
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        printf("i::::%d\n", i);
        print(&SM[i]);
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
    // V(sem1);
    // P(sem2);
    if (sockinfo->sockfd != -1)
    {
        // printf("sockinfo->sockfd is %d\n", sockinfo->sockfd);
        memset(&sockinfo, 0, sizeof(sockinfo));
        return 0;
    }
    else
    {
        memset(&sockinfo, 0, sizeof(sockinfo));
        errno = ENOBUFS;
        exit(EXIT_FAILURE);
    }
    shmdt(SM);
    shmdt(sockinfo);
    return 0;
}

int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int key_sockinfo = ftok("/home/", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);

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

    char ip[20];
    strcpy(ip, inet_ntoa(addr->sin_addr));
    int port = ntohs(addr->sin_port);

    if ((strcmp(SM[i].ip, ip) == 0 && SM[i].port == port))
    {
        printf("IP and port are same\n");
        // write message to the sender side buffer
        send_packet *spkt = (send_packet *)malloc(sizeof(send_packet));
        char new_buf[MAX_FRAME_SIZE];
        new_buf[0] = DATA_TYPE;
        // get the last sequence number
        int last_seq = SM[i].last_ack;
        // update the sequence number
        spkt->sequence_number = last_seq;
        SM[i].last_ack = (SM[i].last_ack + 1) % MAX_SEQUENCE_NUMBER;
        // Convert and copy the sequence number to new_buf
        char seq_number[MSG_ID_SIZE]; // Assuming MSG_ID_SIZE is the size of short
        printf("spkt->sequence_number is %d\n", (int)spkt->sequence_number);
        sprintf(seq_number, "%d", (int)spkt->sequence_number);
        strcat(new_buf, seq_number);

        // Copy the message data (buf) to new_buf using strcat
        strcat(new_buf, buf);

        // Copy new_buf to spkt->data using strcpy
        strcpy(spkt->data, new_buf);
        printf("spkt->data is %s\n", spkt->data);
        // update all other fields of spkt
        spkt->type = DATA_TYPE;
        // get time of day and store in spkt->time
        gettimeofday(&spkt->time, NULL);
        // store the destination address in spkt->to_addr
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
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int key_sockinfo = ftok("/home/", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);

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

    if (SM[i].rbuff.front == SM[i].rbuff.rear)
    {
        errno = ENOMSG;
        exit(EXIT_FAILURE);
    }

    recv_packet *rpkt = SM[i].rbuff.buffer[SM[i].rbuff.front];
    SM[i].rbuff.buffer[SM[i].rbuff.front] = NULL;
    SM[i].rbuff.front = (SM[i].rbuff.front + 1) % SM[i].rbuff.size;

    // return no of bytes received
    return len;
}

int m_close(int sockfd)
{
    //     int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);

    //     shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    //     int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    //     sock_info *sockinfo = shmat(shmid_sockinfo, 0, 0);
    //     int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    //     int sem2 = semget(key_sem2, 1, IPC_CREAT | 0666);

    //     struct sembuf pop;
    //     struct sembuf vop;
    //     pop.sem_num = 0;
    //     pop.sem_op = -1;
    //     pop.sem_flg = 0;
    //     vop.sem_num = 0;
    //     vop.sem_op = 1;
    //     vop.sem_flg = 0;

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

int main()
{
    // testing m_socket()
    cur_init();
    int ret1 = m_socket(AF_INET, SOCK_MTP, 0); // working
    printf("%d\n", ret1);
    // run bind
    int ret = m_bind(ret1, "127.0.0.1", 8080, "127.0.0.1", 8080); // working
    printf("%d\n", ret);
    // run sendto
    char *buf = "Hello";
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(8080);
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ret = m_sendto(ret1, buf, strlen(buf), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    printf("%d", ret);
    // run recvfrom
    char *buf2 = (char *)malloc(1024);
    struct sockaddr_in src_addr;
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(8080);
    src_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    socklen_t addrlen;
    addrlen = sizeof(src_addr);
    ret = m_recvfrom(ret1, buf2, 1024, 0, (struct sockaddr *)&src_addr, &addrlen);

    exit(EXIT_SUCCESS);
}