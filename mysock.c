#include "mysock.h"

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
        if (SM[i].is_free)
            break;
    }
    if (i == MAX_SOCKETS)
    {
        errno = ENOBUFS;
    }
    else
    {
        V(sem1);
        P(sem2);
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

    return sockfd;
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
    for(i=0;i<MAX_SOCKETS;i++){
        if(SM[i].sockfd==sockfd) break;
    }

    sockinfo->sockfd=sockfd;
    strcpy(sockinfo->ip,source_ip);

}

int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{

}

int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{

}

int m_close(int sockfd)
{

}

int dropMessage(float p)
{

}

int main()
{

    exit(EXIT_SUCCESS);
}