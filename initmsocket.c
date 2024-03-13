#include "msocket.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <pthread.h>
#define MAX_SOCKETS 25
void *thread_R(void *arg);
void *thread_S(void *arg);
int key_SM = 1;
int key_sockinfo = 2;
int key_sem1 = 3;
int key_sem2 = 4;
#define P(s) semop(s, &pop, 1) /* pop is the structure we pass for doing \
                  the P(s) operation */
#define V(s) semop(s, &vop, 1) /* vop is the structure we pass for doing \
                  the V(s) operation */

void init_sender_buffer(sendBuffer *sendBuf)
{
    sendBuf->front = 0;
    sendBuf->rear = 0;
    sendBuf->size = MAX_BUFFER_SIZE;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++)
    {
        sendBuf->buffer[i] = NULL;
    }
}
void init_receiver_buffer(recvBuffer *recvBuf)
{
    recvBuf->front = 0;
    recvBuf->rear = 0;
    recvBuf->size = MAX_BUFFER_SIZE;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++)
    {
        recvBuf->buffer[i] = NULL;
    }
}
void init_Sender_Window(int swnd_size, Sender_Window *swnd)
{
    swnd->window_size = swnd_size;
    for (int i = 0; i < swnd_size; i++)
    {
        swnd->window[i] = NULL;
    }
}
void init_Receiver_Window(int rwnd_size, Receiver_Window *rwnd)
{
    rwnd->window_size = rwnd_size;
    for (int i = 0; i < rwnd_size; i++)
    {
        rwnd->window[i] = NULL;
    }
}
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
// thread R
void *thread_R(void *arg)
{
    sharedMemory *SM = (sharedMemory *)arg;
    // create and initialize semaphore set names
}
void *thread_S(void *arg)
{
    sharedMemory *SM = (sharedMemory *)arg;
    // create and initialize semaphore set names
}
// initializes two threads R and S, and a shared memory SM for the process P
void init_process() // process P
{
    // create shared memory for A
    sharedMemory *SM;
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    SM = (sharedMemory *)shmat(shmid_A, 0, 0);
    SOCK_INFO *sockinfo;
    int shmid_sockinfo = shmget(key_sockinfo, sizeof(SOCK_INFO), IPC_CREAT | 0666);
    sockinfo = (SOCK_INFO *)shmat(shmid_sockinfo, 0, 0);
    // initilaize the sockinfo structure
    sockinfo->sock_id = 0;
    sockinfo->ip_address = NULL;
    sockinfo->port = 0;
    // create two semaphores 1 and 2 sem1 and sem2
    int sem1 = semget(key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget(key_sem1, 1, IPC_CREAT | 0666);
    // initialize the semaphores
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    struct sembuf pop, vop;
    // initialize shared memory
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].is_free = 1;
        SM[i].process_id = -1;
        SM[i].udp_socket_id = -1;
        SM[i].ip_address = NULL;
        SM[i].port = -1;
        // initialize send buffer
        SM[i].send_buffer = (sendBuffer *)malloc(sizeof(sendBuffer));
        init_sender_buffer(SM[i].send_buffer);
        // initialize receive buffer
        SM[i].recv_buffer = (recvBuffer *)malloc(sizeof(recvBuffer));
        init_receiver_buffer(SM[i].recv_buffer);
        // initialize sender window
        SM[i].sender_window = (Sender_Window *)malloc(sizeof(Sender_Window));
        init_Sender_Window(MAX_WINDOW_SIZE, SM[i].sender_window);
        // initialize receiver window
        SM[i].receiver_window = (Receiver_Window *)malloc(sizeof(Receiver_Window));
        init_Receiver_Window(MAX_WINDOW_SIZE, SM[i].receiver_window);
    }
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;

    // create thread R
    pthread_t thread_R;
    pthread_create(&thread_R, NULL, thread_R, (void *)SM);
    // create thread S
    pthread_t thread_S;
    pthread_create(&thread_S, NULL, thread_S, (void *)SM);
    // create thread G
    pthread_t thread_G;
    pthread_create(&thread_G, NULL, thread_G, (void *)SM);
    // wait for threads to finish
    pthread_join(thread_R, NULL);
    pthread_join(thread_S, NULL);

    while (1)
    {
        // wait for sem1
        P(sem1);
        // look at SOCK_INFO and find the socket id
        // check if all fields are 0 it is a m_socket call
        if (sockinfo->sock_id == 0 && sockinfo->ip_address == NULL && sockinfo->port == 0 && sockinfo->error_no == 0)
        {
            // create a new socket
            int sock_id = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock_id == -1)
            {
                printf("Error creating socket\n");
                sockinfo->sock_id = -1;
                sockinfo->error_no = errno;
                //
            }
            sockinfo->sock_id = sock_id;
            // signal sem2
            V(sem2);
        }

        else if (sockinfo->sock_id != 0 && sockinfo->ip_address != NULL && sockinfo->port != 0) // it is a m_bind call
        {
            // make a bind call
            struct sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(sockinfo->port);
            server.sin_addr.s_addr = inet_addr(sockinfo->ip_address);
            int bind_status = bind(sockinfo->sock_id, (struct sockaddr *)&server, sizeof(server));
            if (bind_status == -1)
            {
                sockinfo->sock_id = -1;
                sockinfo->error_no = errno;
            }
            // signal sem2
            V(sem2);
        }
    }
}

int main()
{
    init_process();
    return 0;
}