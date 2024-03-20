#include "mysock.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
void *thread_R(void *arg);
void *thread_S(void *arg);

int key_SM = 89;
int key_sockinfo = 90;
int key_sem1 = 91;
int key_sem2 = 92;
#define P(s) semop(s, &pop, 1) /* pop is the structure we pass for doing \
                  the P(s) operation */
#define V(s) semop(s, &vop, 1) /* vop is the structure we pass for doing \
                  the V(s) operation */

// thread R
void *thread_R(void *arg)
{

}
// thread S
void *thread_S(void *arg)
{

}
// thread G
void *thread_G(void *arg)
{

}
void init_process()
{
    printf("Shared Memory\n");

    int shmid_A = shmget(9, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);
    if(shmid_A == -1)
    {
        perror("shmget");
        exit(1);
    }

    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);

    int shmid_sockinfo = shmget(10, 1*sizeof(sock_info), IPC_CREAT | 0666);
    if(shmid_sockinfo == -1)
    {
        perror("shmget");
        exit(1);
    }
    sock_info *sockinfo = (sock_info *)shmat(shmid_sockinfo, 0, 0);

    // Initialize the sockinfo structure
    sockinfo->error_no = 0;
    sockinfo->sock_id = 0;
    sockinfo->port = 0;

    // memset(sockinfo->ip, 0, sizeof(sockinfo->ip));

    // create two semaphores 1 and 2 sem1 and sem2
    int sem1 = semget(11, 1, IPC_CREAT | 0666);
    int sem2 = semget(12, 1, IPC_CREAT | 0666);
    // initialize the semaphores
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    // struct sembuf pop, vop;
    // initialize shared memory
    printf("Initializing shared memory\n");
    for(int i=0;i<MAX_SOCKETS;i++)
    {
        // allocate memory for each socket
        printf("%d",i);
        SM[i].sockfd = -1;
        SM[i].port = -1;
        memset(SM[i].ip, 0, sizeof(SM[i].ip));
        SM[i].is_free= 1;
        SM[i].pid = -1;
        // intialize the sendbuffer and recvbuffer
        printf("Initializing send and recv buffer\n");
        SM[i].sbuff.front = 0;
        SM[i].sbuff.rear = 0;
        SM[i].sbuff.size = 0;
        SM[i].rbuff.front = 0;
        SM[i].rbuff.rear = 0;
        SM[i].rbuff.size = 0;
        for(int j=0;j<MAX_BUFFER_SIZE;j++)
        {
            SM[i].sbuff.buffer[j] = NULL;
            SM[i].rbuff.buffer[j] = NULL;
        }
        // initialize the sender window and receiver window
        SM[i].swnd.size = 0;
        SM[i].rwnd.size = 0;
        for(int j=0;j<MAX_WINDOW_SIZE;j++)
        {
            SM[i].swnd.window[j] = NULL;
            SM[i].rwnd.window[j] = NULL;
        }
        SM[i].swnd.front=SM[i].swnd.rear=0;
        SM[i].rwnd.front=SM[i].rwnd.rear=0;
        SM[i].swnd.size = 0;
        SM[i].rwnd.size = 0;
    }
    struct sembuf pop, vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;

    // create thread R
    pthread_t thread_R;
    pthread_create(&thread_R, NULL, (void *)thread_R, (void *)SM);
    // create thread S
    pthread_t thread_S;
    pthread_create(&thread_S, NULL, thread_S, (void *)SM);
    // create thread G
    pthread_t thread_G;
    pthread_create(&thread_G, NULL, thread_G, (void *)SM);
    // wait for threads to finish
    pthread_join(thread_R, NULL);
    pthread_join(thread_S, NULL);
    printf("Initiating process\n");
    while (1)
    {
        // wait for sem1
        P(sem1);
        // look at SOCK_INFO and find the socket id
        // check if all fields are 0 it is a m_socket call
        if (sockinfo->sock_id == 0 &&  sockinfo->port == 0 && sockinfo->error_no == 0)
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

        else if (sockinfo->sock_id != 0 &&  sockinfo->port != 0) // it is a m_bind call
        {
            // make a bind call
            struct sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(sockinfo->port);
            server.sin_addr.s_addr = inet_addr(sockinfo->ip);
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
    printf("Initiating process\n");
    init_process();
    return 0;
}