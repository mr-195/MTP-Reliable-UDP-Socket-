#ifndef _MSOCKET_H
#define _MSOCKET_H

#define MAX_WINDOW_SIZE 5
#define MAX_BUFFER_SIZE 10

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

// Structure to represent a message
typedef struct
{
    int sequence_number;
    char type; // 'A' for ACK, 'D' for Data
    char data[1024]; // Assuming message size is 1 KB
    // Add other metadata fields as needed
} Message;

// Structure to represent a send_packet
typedef struct
{
    Message message;
    struct sockaddr_in to_addr;

} sendPkt;
// Structure to represent a recv_packet
typedef struct
{
    struct sockaddr_in from_addr;
    Message message;
} recvPkt;

// Element of Sender Window
typedef struct {
    struct timeval time;
    sendPkt packet;
} unAckPkt;

// Sender Buffer 
typedef struct {
    int front;
    int rear;
    int size;
    sendPkt *buffer[MAX_BUFFER_SIZE];
} sendBuffer;

// Receiver Buffer
typedef struct {
    int front;
    int rear;
    int size;
    recvPkt *buffer[MAX_BUFFER_SIZE];
} recvBuffer;

// Structure to represent a Sender_Window 
typedef struct {
    int window_size;
    unAckPkt *window [MAX_WINDOW_SIZE];
} Sender_Window;

// Structure to represent a Receiver_Window
typedef struct {
    int window_size;
    recvPkt *window[MAX_WINDOW_SIZE];
} Receiver_Window;

// Functions available to application

int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);
int dropMessage(float p);



#endif