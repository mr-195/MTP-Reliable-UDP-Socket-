#include "msocket.h"
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_BUFFER_SIZE 10
#define MAX_WINDOW_SIZE 5
#define ACK_TYPE 'A'
#define DATA_TYPE 'D'
#define TYPE_SIZE sizeof(char)
#define MSG_ID_SIZE sizeof(short)
#define MAX_FRAME_SIZE 1024
#define T 1000
#define P(s) semop(s, &pop, 1) /* pop is the structure we pass for doing \
                  the P(s) operation */
#define V(s) semop(s, &vop, 1) /* vop is the structure we pass for doing \
                  the V(s) operation */

#define key_SM  1
#define key_sockinfo  2
#define key_sem1  3
#define key_sem2 4
// last acknowledged sequence number
// global errorno
int ERROR;

void *thread_S(void *arg);
int msg_cntr = 0; // message counter to keep track of the next sequence number
struct sockaddr_in dest_addr;

// thread R

// thread S

int m_socket(int domain, int type, int protocol)
{
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
    // attach to the shared memory SM
    sharedMemory *SM;
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    sharedMemory *SM = (sharedMemory *)shmat(shmid_A, 0, 0);
    SOCK_INFO *sockinfo;
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    // attach to the semaphores create by the main thread
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

    // check for type
    if (type != SOCK_MTP)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    // create a socket
    printf("Creating socket test\n");
    int sockfd = socket(domain, SOCK_DGRAM, protocol);

    if (sockfd >= 0)
    {
        // check whether any free entry is available in the shared memory
        int i;
        for (i = 0; i < MAX_SOCKETS; i++)
        {
            if (SM[i].is_free == 1)
            {
                break;
            }
        }
        if (i == MAX_SOCKETS)
        {
            // set global ERROR to ENOBUFS
            ERROR = ENOBUFS;
            errno = ENOBUFS;
            return -1;
        }
        else // found a free entry in the shared memory at index i
        {
            // signal the semaphore sem1
            V(sem1);
            // wait for the semaphore sem2
            P(sem2);
            // check sockid field of the sockinfo structure
            if (sockinfo->sock_id != -1)
            {
                // reset the fields of the sockinfo structure
                sockinfo->sock_id = 0;
                sockinfo->ip_address = NULL;
                sockinfo->port = 0;
                // put sockfd in the SM table at index i
                SM[i].udp_socket_id = sockfd;
                return sockfd;
            }
            else
            {
                // reset the fields of the sockinfo structure
                sockinfo->sock_id = 0;
                sockinfo->ip_address = NULL;
                sockinfo->port = 0;
                // set global ERROR to ENOBUFS
                ERROR = ENOBUFS;
                errno = ENOBUFS;
                return -1;
            }
        }
    }
    else
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    return sockfd;
}
// bind function
int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port)
{
    // attach to the shared memory SM
    sharedMemory *SM;
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    SM = (sharedMemory *)shmat(shmid_A, 0, 0);
    SOCK_INFO *sockinfo;
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    // attach to the semaphores create by the main thread
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
    // find the corresponding entry in the shared memory
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].udp_socket_id == sockfd)
        {
            break;
        }
    }
    // Put the UDP socket ID, IP, and port in SOCK_INFO table
    sockinfo->sock_id = sockfd;
    sockinfo->ip_address = source_ip;
    sockinfo->port = source_port;
    // signal the semaphore sem1
    V(sem1);
    // wait for the semaphore sem2
    P(sem2);
    if (sockinfo->sock_id != -1)
    {
        // reset all fields of the sockinfo structure
        sockinfo->sock_id = 0;
        sockinfo->ip_address = NULL;
        sockinfo->port = 0;

        return 1;
    }
    else
    {
        // reset all fields of the sockinfo structure
        sockinfo->sock_id = 0;
        sockinfo->ip_address = NULL;
        sockinfo->port = 0;
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
}

int m_sendto(int sockfd, const void *buf, size_t len, int flags,
             const struct sockaddr *client_addr, socklen_t addrlen)
{
    // attach to the shared memory SM
    sharedMemory *SM;
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    SM = (sharedMemory *)shmat(shmid_A, 0, 0);
    SOCK_INFO *sockinfo;
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    // attach to the semaphores create by the main thread
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
    // find the corresponding entry in the shared memory
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].udp_socket_id == sockfd)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    // get ip address and port from the client address
    // check for space in the send buffer
    if ((SM[i].send_buffer->rear + 1) % SM[i].send_buffer->size == SM[i].send_buffer->front)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    struct sockaddr_in *addr = (struct sockaddr_in *)client_addr;
    char *ip = inet_ntoa(addr->sin_addr);
    int port = ntohs(addr->sin_port);
    // check if the ipaddress match with the ipaddress in the shared memory
    if (SM[i].ip_address == ip && SM[i].port == port)
    {
        // write message to the sender side buffer
        sendPkt *spkt = (sendPkt *)malloc(sizeof(sendPkt));
        char new_buf[MAX_FRAME_SIZE];
        int offset = 0;

        // Copy the DATA_TYPE to new_buf
        strcpy(new_buf, DATA_TYPE);
        offset += strlen(DATA_TYPE); // Update offset

        // Convert and copy the sequence number to new_buf
        short seq_number = htons(spkt->message.sequence_number); // Assuming sequence_number is short
        memcpy(new_buf + offset, &seq_number, MSG_ID_SIZE);
        offset += MSG_ID_SIZE; // Update offset

        // Copy the message data (buf) to new_buf
        memcpy(new_buf + offset, buf, len);
        offset += len; // Update offset

        // Copy new_buf to spkt->message.data
        memcpy(spkt->message.data, new_buf, offset);

        spkt->to_addr = *addr;
        SM[i].send_buffer->buffer[SM[i].send_buffer->rear] = spkt;
        SM[i].send_buffer->rear = (SM[i].send_buffer->rear + 1) % SM[i].send_buffer->size;
    }
    else
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
   
    return len;
}
/* m_recvfrom – looks up the receiver-side message buffer to see if any message is
already received. If yes, it returns the first message (in-order) and deletes that
message from the table. If not, it returns with -1 and sets a global error variable to
ENOMSG, indicating no message has been available in the message buffer. So the
m_recvfrom call is non-blocking.*/
int m_recvfrom(int sockfd, void *buf, size_t len, int flags,
               struct sockaddr *client_addr, socklen_t *addrlen)
{
      // attach to the shared memory SM
    sharedMemory *SM;
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    SM = (sharedMemory *)shmat(shmid_A, 0, 0);
    SOCK_INFO *sockinfo;
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    // attach to the semaphores create by the main thread
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
    // find the corresponding entry in the shared memory
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].udp_socket_id == sockfd)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    // check for space in the receive buffer
    if (SM[i].recv_buffer->front == SM[i].recv_buffer->rear)
    {
        // set global ERROR to ENOMSG
        ERROR = ENOMSG;
        errno = ENOMSG;
        return -1;
    }
    // get the first message from the receiver side buffer
    recvPkt *rpkt ;
    rpkt = SM[i].recv_buffer->buffer[SM[i].recv_buffer->front];
    // delete the message from the receiver side buffer
    SM[i].recv_buffer->buffer[SM[i].recv_buffer->front] = NULL;
    SM[i].recv_buffer->front = (SM[i].recv_buffer->front + 1) % SM[i].recv_buffer->size;


}
int m_close(int sockfd)
{
    sharedMemory *SM;
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    SM = (sharedMemory *)shmat(shmid_A, 0, 0);
    SOCK_INFO *sockinfo;
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    // attach to the semaphores create by the main thread
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

    // find the corresponding entry in the shared memory
    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (SM[i].udp_socket_id == sockfd)
        {
            break;
        }
    }
    if (i == MAX_SOCKETS)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    // close the socket
    // mark the entry in the shared memory as free
    SM[i].is_free = 1;
    int ret = close(sockfd);
    return ret;
}