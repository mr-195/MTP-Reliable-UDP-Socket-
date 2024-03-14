#ifndef _MSOCKET_H
#define _MSOCKET_H

#define MAX_WINDOW_SIZE 5
#define MAX_BUFFER_SIZE 10
#define MAX_SOCKETS 25
#define SOCK_MTP 15

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
    short sequence_number;
    char type;       // 'A' for ACK, 'D' for Data
    char data[1024]; // Assuming message size is 1 KB which also includes the (header + sequence number + data)
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
typedef struct
{
    struct timeval time;
    sendPkt packet;
} unAckPkt;

// Sender Buffer
typedef struct
{
    int front;
    int rear;
    int size;
    sendPkt *buffer[MAX_BUFFER_SIZE];
} sendBuffer;

// Receiver Buffer
typedef struct
{
    int front;
    int rear;
    int size;
    recvPkt *buffer[MAX_BUFFER_SIZE];
} recvBuffer;

// Structure to represent a Sender_Window
typedef struct
{
    int window_size;
    unAckPkt *window[MAX_WINDOW_SIZE];
} Sender_Window;

// Structure to represent a Receiver_Window
typedef struct
{
    int window_size;
    recvPkt *window[MAX_WINDOW_SIZE];
} Receiver_Window;

// element of Shared Memory
typedef struct {
    int is_free;       // Indicates whether the MTP socket is free or allotted
    pid_t process_id;                   // Process ID of the process that created the MTP socket
    int udp_socket_id;                // Corresponding UDP socket ID
    char *ip_address;                 // IP address of the other end of the MTP socket (assuming IPv4)
    int port;                         // Port number of the other end of the MTP socket
    sendBuffer *send_buffer;          // Send Buffer
    recvBuffer *recv_buffer;          // Receive Buffer
    Sender_Window *sender_window;     // Sender Window
    Receiver_Window *receiver_window; // Receiver Window
} sharedMemory;

// element of SOCK_INFO
typedef struct
{
    int sock_id;
    char *ip_address;
    int port;
    int error_no;
} SOCK_INFO;
// Functions available to application

int m_socket(int domain, int type, int protocol);
int m_bind(int sockfd, const char *source_ip, int source_port, const char *dest_ip, int dest_port);
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int m_close(int sockfd);
int dropMessage(float p);

#endif