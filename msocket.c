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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define SOCK_MTP 0x8
#define MAX_BUFFER_SIZE 10
#define MAX_WINDOW_SIZE 5
#define ACK_TYPE 'A'
#define DATA_TYPE 'D'
#define ENOTBOUND 1
#define TYPE_SIZE sizeof(char)
#define MSG_ID_SIZE sizeof(short)
#define T 1000
#define P(s) semop(s, &pop, 1) /* pop is the structure we pass for doing \
                  the P(s) operation */
#define V(s) semop(s, &vop, 1) /* vop is the structure we pass for doing \
                  the V(s) operation */
int key_SM = 1;
int key_sockinfo = 2;
int key_sem1 = 3;
int key_sem2 = 4;

sendBuffer *sendBuf;
recvBuffer *recvBuf;
Sender_Window *swnd;
Receiver_Window *rwnd;
pthread_t tid_R, tid_S;
int flag_nospace = 0;
int last_ack_seq = -1; // last acknowledged sequence number
// global errorno
int ERROR;
void *thread_R(void *arg);
void *thread_S(void *arg);
int msg_cntr = 0; // message counter to keep track of the next sequence number
struct sockaddr_in dest_addr;

void cleanup()
{
    free(sendBuf->buffer);
    free(recvBuf->buffer);
    free(swnd->window);
    free(rwnd->window);
    free(sendBuf);
    free(recvBuf);
    free(swnd);
    free(rwnd);
}
// thread R
void *thread_R(void *arg)
{
    int sockfd = *((int *)arg);
    fd_set readfds;
    struct timeval timeout;
    int maxfd = sockfd + 1;
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int status = select(maxfd, &readfds, NULL, NULL, &timeout);
        if (status < 0)
        {
            // set global ERROR to EAGAIN
            ERROR = EAGAIN;
            errno = EAGAIN;
            return NULL;
        }
        else if (status > 0)
        {
            // wait for a message to come
            recvPkt *rpkt = (recvPkt *)malloc(sizeof(recvPkt));
            // from address is the dest_addr
            rpkt->from_addr = dest_addr;
            int len = sizeof(rpkt->from_addr);
            int n = recvfrom(sockfd, rpkt->message.data, sizeof(rpkt->message.data), 0, (struct sockaddr *)&rpkt->from_addr, &len);
            if (n < 0)
            {
                // set global ERROR to EAGAIN
                ERROR = EAGAIN;
                errno = EAGAIN;
                return NULL;
            }
            else if (n > 0)
            {
                // check if the message is an ACK
                if (rpkt->message.data[0] == ACK_TYPE)
                {
                    // update the sender window
                    int ack_seq;
                    memcpy(&ack_seq, rpkt->message.data + TYPE_SIZE, MSG_ID_SIZE);
                    for (int i = 0; i < swnd->window_size; i++)
                    {
                        if (swnd->window[i] != NULL && swnd->window[i]->packet.message.sequence_number == ack_seq)
                        {
                            free(swnd->window[i]);
                            swnd->window[i] = NULL;
                            break;
                        }
                    }
                }
                else if (rpkt->message.data[0] == DATA_TYPE)
                {
                    // add the message to the receiver buffer
                    recvBuf->buffer[recvBuf->rear] = rpkt;
                    recvBuf->rear = (recvBuf->rear + 1) % recvBuf->size;
                    // send an ACK
                    Message *ack = (Message *)malloc(sizeof(Message));
                    ack->type = ACK_TYPE;
                    ack->sequence_number = rpkt->message.sequence_number;
                    last_ack_seq = rpkt->message.sequence_number;
                    ack->data[0] = ACK_TYPE;
                    short t = htons(ack->sequence_number);
                    memcpy(ack->data + TYPE_SIZE, &t, MSG_ID_SIZE);
                    sendto(sockfd, ack->data, sizeof(ack->data), 0, (struct sockaddr *)&rpkt->from_addr, sizeof(rpkt->from_addr));
                    // set flag nospace if the available space at the receive buffer is zero
                    if (recvBuf->front == recvBuf->rear)
                    {
                        // set flag nospace
                        flag_nospace = 1;
                    }
                }
                else // if there is a timeout
                {
                    // check if the flag nospace was set but now there is space available in the receive buffer
                    if (flag_nospace == 1 && recvBuf->front != recvBuf->rear)
                    {
                        // send a duplicate ACK message with the last acknowledged sequence number but with the updated rwnd size
                        Message *ack = (Message *)malloc(sizeof(Message));
                        ack->type = ACK_TYPE;
                        // get the last acknowledged sequence number
                        ack->sequence_number = last_ack_seq;
                        ack->data[0] = ACK_TYPE;
                        short t = htons(ack->sequence_number);
                        memcpy(ack->data + TYPE_SIZE, &t, MSG_ID_SIZE);
                        sendto(sockfd, ack->data, sizeof(ack->data), 0, (struct sockaddr *)&rpkt->from_addr, sizeof(rpkt->from_addr));
                        // reset the flag
                        flag_nospace = 0;
                    }
                }
            }
        }
    }
}
// thread S
void *thread_S(void *arg)
{

    // sleep for time < T/2
    sleep(T / 2);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    while (1)
    {
        gettimeofday(&end, NULL);
        if ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec > T / 2)
        {
            // check for timeout
            for (int i = 0; i < swnd->window_size; i++)
            {
                if (swnd->window[i] != NULL)
                {
                    struct timeval now;
                    gettimeofday(&now, NULL);
                    if ((now.tv_sec - swnd->window[i]->time.tv_sec) * 1000000 + now.tv_usec - swnd->window[i]->time.tv_usec > T)
                    {
                        // retransmit all the messages in the sender window
                        for (int j = 0; j < swnd->window_size; j++)
                        {
                            if (swnd->window[j] != NULL)
                            {
                                sendto(*((int *)arg), swnd->window[j]->packet.message.data, sizeof(swnd->window[j]->packet.message.data), 0, (struct sockaddr *)&swnd->window[j]->packet.to_addr, sizeof(swnd->window[j]->packet.to_addr));
                                gettimeofday(&swnd->window[j]->time, NULL);
                            }
                        }
                    }
                }
            }
        }
    }
}

int m_socket(int domain, int type, int protocol)
{
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
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

    // check for type
    if (type != SOCK_MTP)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    // create a socket
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
    // check if the destination address matches the bound address
    struct sockaddr_in *addr = (struct sockaddr_in *)client_addr;
    if (addr->sin_family != dest_addr.sin_family || addr->sin_port != dest_addr.sin_port || addr->sin_addr.s_addr != dest_addr.sin_addr.s_addr)
    {
        // set global ERROR to ENOTBOUND
        errno = ENOTBOUND;
        return -1;
    }
    // check if the send buffer is full
    if (sendBuf->front == (sendBuf->rear + 1) % sendBuf->size)
    {
        // set global ERROR to ENOBUFS
        ERROR = ENOBUFS;
        errno = ENOBUFS;
        return -1;
    }
    // create a message
    Message *msg = (Message *)malloc(sizeof(Message));
    msg->sequence_number = msg_cntr;
    msg_cntr++;
    msg->type = DATA_TYPE;
    // add the DATA_TYPE and Sequence number to the message
    msg->data[0] = DATA_TYPE;
    short seq_buf = htons(msg->sequence_number);
    memcpy(msg->data + TYPE_SIZE, &seq_buf, MSG_ID_SIZE);
    memcpy(msg->data + TYPE_SIZE + MSG_ID_SIZE, buf, len);
    // create a send packet
    sendPkt *spkt = (sendPkt *)malloc(sizeof(sendPkt));
    spkt->message = *msg;
    spkt->to_addr = *addr;
    // add the packet to the send buffer
    sendBuf->buffer[sendBuf->rear] = spkt;
    sendBuf->rear = (sendBuf->rear + 1) % sendBuf->size;
    return 0;
}
/* m_recvfrom – looks up the receiver-side message buffer to see if any message is
already received. If yes, it returns the first message (in-order) and deletes that
message from the table. If not, it returns with -1 and sets a global error variable to
ENOMSG, indicating no message has been available in the message buffer. So the
m_recvfrom call is non-blocking.*/
int m_recvfrom(int sockfd, void *buf, size_t len, int flags,
               struct sockaddr *client_addr, socklen_t *addrlen)
{
    // check if the receive buffer is empty
    if (recvBuf->front == recvBuf->rear)
    {
        // set global ERROR to ENOMSG
        ERROR = ENOMSG;
        errno = ENOMSG;
        return -1;
    }
    // get the first message from the receive buffer
    recvPkt *rpkt = recvBuf->buffer[recvBuf->front];
    // delete the message from the buffer by setting it to NULL
    recvBuf->buffer[recvBuf->front] = NULL;
    // update the front of the buffer
    recvBuf->front = (recvBuf->front + 1) % recvBuf->size;
    // copy the message to the buffer
    memcpy(buf, rpkt->message.data, len);
    // copy the address to the client address
    struct sockaddr_in *addr = (struct sockaddr_in *)client_addr;
    *addr = rpkt->from_addr;
    // return size of the message
    return sizeof(rpkt->message.data);
}
int m_close(int sockfd)
{
    // close the socket
    int status = close(sockfd);
    // cancel the threads
    pthread_cancel(tid_R);
    pthread_cancel(tid_S);
    // wait for threads
    pthread_join(tid_R, NULL);
    pthread_join(tid_S, NULL);
    // cleanup the memory
    cleanup();
    return status;
}