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
    short sequence_number;
    char type;
    char data[1024];
    struct sockaddr_in to_addr;
} send_packet;

typedef struct
{
    int size;
    int front;
    int rear;
    send_packet *window[MAX_WINDOW_SIZE];
} send_window;

typedef struct
{
    int size;
    int front;
    int rear;
    recv_packet *window[MAX_WINDOW_SIZE];
} recv_window;

typedef struct
{
    int size;
    int front;
    int rear;
    send_packet *buffer[MAX_BUFFER_SIZE];
} send_buff;

typedef struct
{
    int size;
    int front;
    int rear;
    recv_packet *buffer[MAX_BUFFER_SIZE];
} recv_buff;

typedef struct
{
    int is_free;
    pid_t pid;
    int sockfd;
    char ip[20];
    int port;
    send_window swnd;
    send_buff sbuff;
    recv_window rwnd;
    recv_buff rbuff;
} shared_memory;

typedef struct
{
    int sock_id;
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