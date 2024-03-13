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
#define ACK_TYPE 'A'
#define DATA_TYPE 'D'
#define ENOTBOUND 1
#define TYPE_SIZE sizeof(char)
#define MSG_ID_SIZE sizeof(short)
#define T 1000

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
/* The thread S behaves in the following manner. It sleeps for some time ( < T/2 ), and wakes
up periodically. On waking up, it first checks whether the message timeout period (T) is over
(by computing the time difference between the current time and the time when the messages
within the window were sent last) for the messages sent over any of the active MTP sockets.
If yes, it retransmits all the messages within the current swnd for that MTP socket. It then
checks the current swnd for each of the MTP sockets and determines whether there is a
pending message from the sender-side message buffer that can be sent. If so, it sends that
message through the UDP sendto() call for the corresponding UDP socket and updates the
send timestamp .*/
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
    // initilaize element of Sender Window
    void init_unAckPkt(unAckPkt * pkt, sendPkt * spkt)
    {
        pkt->packet = *spkt;
        gettimeofday(&pkt->time, NULL);
    }

    // initialize the receiver packet
    void init_recvPkt(recvPkt * pkt, Message * msg, struct sockaddr_in from_addr)
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