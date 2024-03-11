#include "msocket.h"
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
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
#define MAX_RETRIES 5
#define TIMEOUT 5000

sendBuffer *sendBuf;
recvBuffer *recvBuf;
Sender_Window *swnd;
Receiver_Window *rwnd;
pthread_t tid_R, tid_S;
// global errorno
int ERROR;
void *thread_R(void *arg);
void *thread_S(void *arg);
int msg_cntr=0; // message counter to keep track of the next sequence number
struct sockaddr_in dest_addr;

void init_sender_buffer()
{
    sendBuf->front = 0;
    sendBuf->rear = 0;
    sendBuf->size = MAX_BUFFER_SIZE;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++)
    {
        sendBuf->buffer[i] = NULL;
    }
}
void init_Sender_Window(int swnd_size)
{
    swnd->window_size = swnd_size;
    for (int i = 0; i < swnd_size; i++)
    {
        swnd->window[i] = NULL;
    }
}
void init_Receiver_Window(int rwnd_size)
{
    rwnd->window_size = rwnd_size;
    for (int i = 0; i < rwnd_size; i++)
    {
        rwnd->window[i] = NULL;
    }
}
void init_recv_buffer()
{
    recvBuf->front = 0;
    recvBuf->rear = 0;
    recvBuf->size = MAX_BUFFER_SIZE;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++)
    {
        recvBuf->buffer[i] = NULL;
    }
}
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

// void send_message(Message *msg, struct sockaddr_in to_addr) {
//     sendPkt *pkt = (sendPkt *)malloc(sizeof(sendPkt));
//     pkt->message = *msg;
//     pkt->to_addr = to_addr;
//     sendBuf->buffer[sendBuf->rear] = pkt;
//     sendBuf->rear = (sendBuf->rear + 1) % sendBuf->size;
// }
// initilaize element of Sender Window
void init_unAckPkt(unAckPkt *pkt, sendPkt *spkt)
{
    pkt->packet = *spkt;
    gettimeofday(&pkt->time, NULL);
}

// initialize the receiver packet
void init_recvPkt(recvPkt *pkt, Message *msg, struct sockaddr_in from_addr)
{
    pkt->message = *msg;
    pkt->from_addr = from_addr;
}

int m_socket(int domain, int type, int protocol)
{
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
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
    int *sockfd_arg = (int *)malloc(sizeof(int));
    *sockfd_arg = sockfd;

    if (sockfd >= 0)
    {
        // initialize the send buffer and senderwindow
        sendBuf = (sendBuffer *)malloc(sizeof(sendBuffer));
        recvBuf = (recvBuffer *)malloc(sizeof(recvBuffer));
        swnd = (Sender_Window *)malloc(sizeof(Sender_Window));
        rwnd = (Receiver_Window *)malloc(sizeof(Receiver_Window));
        init_sender_buffer();
        init_Sender_Window(MAX_WINDOW_SIZE);
        init_recv_buffer();
        init_Receiver_Window(MAX_WINDOW_SIZE);
        // create a thread for sender R
        if (pthread_create(&tid_R, NULL, thread_R, (void *)sockfd_arg) != 0)
        {
            // set global ERROR to EAGAIN
            ERROR = EAGAIN;
            errno = EAGAIN;
            return -1;
        }
        // create a thread for receiver S
        if (pthread_create(&tid_S, NULL, thread_S, (void *)sockfd_arg) != 0)
        {
            // set global ERROR to EAGAIN
            ERROR = EAGAIN;
            errno = EAGAIN;
            return -1;
        }
    }
    return sockfd;
}
// bind function
int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port)
{
    // bind the socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(source_port);
    addr.sin_addr.s_addr = inet_addr(source_ip);
    int bind_status = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (bind_status < 0)
    {
        // set global ERROR to EADDRINUSE
        ERROR = EADDRINUSE;
        errno = EADDRINUSE;
        return -1;
    }
    // make destinaton address

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    return bind_status;
}
/* writes the message to the sender side message buffer if the destination
IP/Port matches with the bounded IP/Port as set through m_bind(). If not, it drops the
message, returns -1 and sets the global error variable to ENOTBOUND. If there is no
space is the send buffer, return -1 and set the global error variable to ENOBUFS. So
the m_sendto call is non-blocking. */
int m_sendto(int sockfd, const void *buf, size_t len, int flags,
             const struct sockaddr *client_addr, socklen_t addrlen)
{
    // check if the destination address matches the bound address
    struct sockaddr_in *addr = (struct sockaddr_in *)client_addr;
    if (addr->sin_family != dest_addr.sin_family || addr->sin_port != dest_addr.sin_port || addr->sin_addr.s_addr != dest_addr.sin_addr.s_addr)
    {
        // set global ERROR to ENOTBOUND
        ERROR = ENOTBOUND;
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
    memcpy(msg->data, buf, len);
    // create a send packet
    sendPkt *spkt = (sendPkt *)malloc(sizeof(sendPkt));
    spkt->message = *msg;
    spkt->to_addr = *addr;
    // add the packet to the send buffer
    sendBuf->buffer[sendBuf->rear] = spkt;
    sendBuf->rear = (sendBuf->rear + 1) % sendBuf->size;
   
    return 0;
}