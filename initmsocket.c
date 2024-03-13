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
//initializes two threads R and S, and a shared memory SM for the process P
void init_process() // process P
{
  // create shared memory for A
    sharedMemory *SM ;
    int shmid_A = shmget(IPC_PRIVATE, MAX_SOCKETS * sizeof(sharedMemory), IPC_CREAT | 0666);
    SM = (sharedMemory *)shmat(shmid_A,0, 0);
    // initialize shared memory
    for(int i=0;i<MAX_SOCKETS;i++){
        SM[i].is_free = 1;
        SM[i].process_id = -1;
        SM[i].udp_socket_id = -1;
        SM[i].ip_address = NULL;
        SM[i].port = -1;
        SM[i].send_buffer = NULL;
        SM[i].recv_buffer = NULL;
        SM[i].sender_window = NULL;
        SM[i].receiver_window = NULL;
    }
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



}