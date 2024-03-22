#include "mysock.h"
#include <sys/socket.h> // Add the necessary header file for the recvfrom function
void *thread_R(void *arg);
void *thread_S(void *arg);
void *thread_G(void *arg);
int nospace = 0;
// #define key_SM  89
// #define key_sockinfo  90
// #define key_sem1  91
// #define key_sem2  92
#define P(s) semop(s, &pop, 1) /* pop is the structure we pass for doing \
                  the P(s) operation */
#define V(s) semop(s, &vop, 1) /* vop is the structure we pass for doing \
                  the V(s) operation */

// thread R
void *thread_R(void *arg)
{
    // shared_memory *SM = (shared_memory *)arg;
    //
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);
    int key_sockinfo = ftok("/home/", 'B');
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = (sock_info *)shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    int key_sem3 = ftok("/home/", 'E'); // semaphore for shared memory SM
    int sem_SM = semget((key_t)key_sem3, 1, IPC_CREAT | 0666);
    printf("Thread R\n");
    fd_set readfds;
    struct timeval timeout;
    while (1)
    {
        FD_ZERO(&readfds);
        int max_fd = 0;
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (SM[i].is_free == 0)
            {
                FD_SET(SM[i].sockfd, &readfds);
                if (SM[i].sockfd > max_fd)
                {
                    max_fd = SM[i].sockfd;
                }
            }
        }
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        int ret = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                if (FD_ISSET(SM[i].sockfd, &readfds))
                {
                    // receive the packet
                    recv_packet *pkt = (recv_packet *)malloc(sizeof(recv_packet));
                    pkt->from_addr.sin_family = AF_INET;
                    pkt->from_addr.sin_port = htons(SM[i].port);
                    pkt->from_addr.sin_addr.s_addr = inet_addr(SM[i].ip);
                    int len = sizeof(pkt->from_addr);
                    char buf[MAX_FRAME_SIZE];
                    

                    int n = recvfrom(SM[i].sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&pkt->from_addr, &len);
                    if (n == -1)
                    {
                        printf("Error receiving packet\n");
                    }
                    else
                    {
                        // check if it is an DATA packet store in the reciver side message buffer
                        if (buf[0] == DATA_TYPE)
                        {
                            pkt->type = DATA_TYPE;
                            // extract the sequence number
                            short seq_num;
                            seq_num = ntohs(*(short *)(buf + TYPE_SIZE));
                            pkt->sequence_number = seq_num;
                            // store buf in pkt->message.data
                            for (int i = 0; i < MAX_FRAME_SIZE; i++)
                            {
                                pkt->data[i] = buf[i];
                            }

                            // add the packet to the receive buffer
                            SM[i].rbuff.buffer[SM[i].rbuff.rear] = pkt;
                            SM[i].rbuff.rear = (SM[i].rbuff.rear + 1) % MAX_BUFFER_SIZE;
                            SM[i].rbuff.size++;

                            // send ACK for the received packet
                            char ack_buf[MAX_FRAME_SIZE];
                            ack_buf[0] = ACK_TYPE;
                            short t = htons(seq_num);
                            memcpy(ack_buf + TYPE_SIZE, &t, MSG_ID_SIZE);
                            int n = sendto(SM[i].sockfd, ack_buf, MAX_FRAME_SIZE, 0, (struct sockaddr *)&pkt->from_addr, len);
                            // set flag nospace if receiver buffer is full
                            if (SM[i].rbuff.size == MAX_BUFFER_SIZE)
                            {
                                SM[i].flag_nospace = 1;
                            }
                            // update the last acknowledged packet
                            SM[i].last_ack = seq_num;
                        }
                        else if (buf[0] == ACK_TYPE)
                        {
                            // extract the sequence number
                            short seq_num;
                            seq_num = ntohs(*(short *)(buf + TYPE_SIZE));
                            // remove the packet from the sender window
                            for (int j = 0; j < SM[i].swnd.size; j++)
                            {

                                if (SM[i].swnd.window[j]->sequence_number == seq_num)
                                {
                                    if (SM[i].swnd.window[j] == NULL) // duplicate message
                                    {
                                        // update the size of the sender window size
                                        SM[i].swnd.size--;
                                        break;
                                    }
                                    else // first ACK message for the packet
                                    {
                                        // set to NULL
                                        SM[i].swnd.window[j] = NULL;
                                        // remove the message from the sender buffer
                                        for (int k = 0; k < SM[i].sbuff.size; k++)
                                        {
                                            if (SM[i].sbuff.buffer[k]->sequence_number == seq_num)
                                            {
                                                SM[i].sbuff.buffer[k] = NULL;
                                                SM[i].sbuff.size--;
                                                break;
                                            }
                                        }
                                        // update the size of the sender window size
                                        SM[i].swnd.size--;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else // case of time out or no packet received
        {
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                // check if flag no space was set
                if (SM[i].flag_nospace == 1)
                {
                    // get the last acknowledged packet
                    // send
                    int last_ack = SM[i].last_ack;
                    // send ACK for the last acknowledged packet
                    char ack_buf[MAX_FRAME_SIZE];
                    ack_buf[0] = ACK_TYPE;
                    short t = htons(last_ack);
                    memcpy(ack_buf + TYPE_SIZE, &t, MSG_ID_SIZE);
                    struct sockaddr_in from_addr;
                    from_addr.sin_family = AF_INET;
                    from_addr.sin_port = htons(SM[i].port);
                    from_addr.sin_addr.s_addr = inet_addr(SM[i].ip);
                    int len = sizeof(from_addr);
                    int n = sendto(SM[i].sockfd, ack_buf, MAX_FRAME_SIZE, 0, (struct sockaddr *)&from_addr, len);
                    if (n == -1)
                    {
                        printf("Error sending ACK\n");
                    }
                    // update the reciever window
                    // doubt in this part
                    SM[i].rwnd.window[SM[i].rwnd.rear] = SM[i].rbuff.buffer[SM[i].rwnd.rear];
                    SM[i].rwnd.rear = (SM[i].rwnd.rear + 1) % MAX_WINDOW_SIZE;
                }
            }
        }
    }
}
// thread S
void *thread_S(void *arg)
{
    int key_SM = ftok("/home/", 'A');
    int shmid_A = shmget((key_t)key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);
    int key_sockinfo = ftok("/home/", 'B');
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = (sock_info *)shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok("/home/", 'C');
    int key_sem2 = ftok("/home/", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    int key_sem3 = ftok("/home/", 'E'); // semaphore for shared memory SM
    int sem_SM = semget((key_t)key_sem3, 1, IPC_CREAT | 0666);
    printf("Thread S\n");
    while (1)
    {
        sleep(T / 2);
        // get current time
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            // loop through the sender window
            for (int j = 0; j < SM[i].swnd.size; j++)
            {
                // check if the time difference between the current time and the time of the packet is greater than T
                struct timeval diff;
                timersub(&current_time, &SM[i].swnd.window[j]->time, &diff);
                if (diff.tv_sec > T)
                {
                    // restransmit all the packets in the sender window
                    for (int k = 0; k < SM[i].swnd.size; k++)
                    {
                        if (SM[i].swnd.window[k] != NULL)
                        {
                            // send the packet
                            struct sockaddr_in to_addr;
                            to_addr.sin_family = AF_INET;
                            to_addr.sin_port = htons(SM[i].port);
                            to_addr.sin_addr.s_addr = inet_addr(SM[i].ip);
                            int len = sizeof(to_addr);
                            int n = sendto(SM[i].sockfd, SM[i].swnd.window[k]->data, MAX_FRAME_SIZE, 0, (struct sockaddr *)&to_addr, len);
                            if (n == -1)
                            {
                                printf("Error sending packet\n");
                            }
                        }
                    }
                    break;
                }
            }
        }
        // sleep for T/2
    }
}
// thread G
void *thread_G(void *arg)
{
    return NULL;
}
void init_process()
{
    struct timeval seed;
    gettimeofday(&seed, NULL);
    srand(seed.tv_usec);
    key_t key_SM = ftok(".", 'A');
    int shmid_A = shmget(key_SM, MAX_SOCKETS * sizeof(shared_memory), IPC_CREAT | 0666);
    shared_memory *SM = (shared_memory *)shmat(shmid_A, 0, 0);
    printf(" key_SM = %d\n", key_SM);
    int key_sockinfo = ftok(".", 'B');
    int shmid_sockinfo = shmget((key_t)key_sockinfo, sizeof(sock_info), IPC_CREAT | 0666);
    sock_info *sockinfo = (sock_info *)shmat(shmid_sockinfo, 0, 0);
    int key_sem1 = ftok(".", 'C');
    int key_sem2 = ftok(".", 'D');
    int sem1 = semget((key_t)key_sem1, 1, IPC_CREAT | 0666);
    int sem2 = semget((key_t)key_sem2, 1, IPC_CREAT | 0666);
    int key_sem3 = ftok(".", 'E'); // semaphore for shared memory SM
    printf(" key_sem3 = %d\n", key_sem3);
    int sem_SM = semget((key_t)key_sem3, 1, IPC_CREAT | 0666);
    
    // initialize the semaphores
    semctl(sem1, 0, SETVAL, 0);
    semctl(sem2, 0, SETVAL, 0);
    semctl(sem_SM, 0, SETVAL, 1);
    struct sembuf pop, vop;
    pop.sem_num = 0;
    pop.sem_op = -1;
    pop.sem_flg = 0;
    vop.sem_num = 0;
    vop.sem_op = 1;
    vop.sem_flg = 0;
    P(sem_SM);
    sockinfo->sockfd = 0;
    sockinfo->port = 0;
    sockinfo->ip[0] = '\0';
    sockinfo->error_no = 0;
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].sockfd = -1;
        SM[i].port = -1;
        memset(SM[i].ip, 0, sizeof(SM[i].ip));
        SM[i].is_free = 1;
        SM[i].pid = -1;
        SM[i].flag_nospace = 0;
        SM[i].last_ack = 0;
        SM[i].sbuff.front = 0;
        SM[i].sbuff.rear = 0;
        SM[i].sbuff.size = 0;
        SM[i].rbuff.front = 0;
        SM[i].rbuff.rear = 0;
        SM[i].rbuff.size = 0;
        // intialize the sender window
        SM[i].swnd.front = 0;
        SM[i].swnd.rear = 0;
        SM[i].swnd.size = 0;
        // intialize the receiver window
        SM[i].rwnd.front = 0;
        SM[i].rwnd.rear = 0;
        SM[i].rwnd.size = 0;

        for (int j = 0; j < MAX_WINDOW_SIZE; j++)
        {
            SM[i].swnd.window[j] = NULL;
            SM[i].rwnd.window[j] = NULL;
        }
        for (int j = 0; j < MAX_BUFFER_SIZE; j++)
        {
            SM[i].sbuff.buffer[j] = NULL;
            SM[i].rbuff.buffer[j] = NULL;
        }
    }
    recv_packet *rpkt = (recv_packet *)malloc(sizeof(recv_packet));
    rpkt->sequence_number = 0;
    rpkt->type = DATA_TYPE;
    rpkt->from_addr.sin_family = AF_INET;
    rpkt->from_addr.sin_port = htons(8080);
    rpkt->from_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    strcpy(rpkt->data, "Hello\0");
    SM[0].rbuff.buffer[0] = rpkt;
    printf("SM[0].rbuff.buffer[0]->data = %s\n", SM[0].rbuff.buffer[0]->data);
    // detach 
    // shmdt(SM);
    V(sem_SM);

    // create thread R
    pthread_t threadR;
    pthread_create(&threadR, NULL, &thread_R, NULL);
    // pthread_join(threadR, NULL);
    // create thread S
    pthread_t threadS;
    pthread_create(&threadS, NULL, &thread_S, NULL);
    // // create thread G
    pthread_t threadG;
    pthread_create(&threadG, NULL, &thread_G, NULL);
    // wait for threads to finish
    pthread_join(threadR, NULL);
    pthread_join(threadS, NULL);
    pthread_join(threadG, NULL);
    printf("Initiating process\n");
    while (1)
    {
        // wait for sem1
        P(sem1);
        // look at SOCK_INFO and find the socket id
        // check if all fields are 0 it is a m_socket call
        if (sockinfo->sockfd == 0 && sockinfo->port == 0 && sockinfo->error_no == 0)
        {
            // create a new socket
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd == -1)
            {
                printf("Error creating socket\n");
                sockinfo->sockfd = -1;
                sockinfo->error_no = errno;
                //
            }
            sockinfo->sockfd = sockfd;
            // signal sem2
            V(sem2);
        }

        else if (sockinfo->sockfd != 0 && sockinfo->port != 0) // it is a m_bind call
        {
            // make a bind call
            struct sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(sockinfo->port);
            server.sin_addr.s_addr = inet_addr(sockinfo->ip);
            int bind_status = bind(sockinfo->sockfd, (struct sockaddr *)&server, sizeof(server));
            if (bind_status == -1)
            {
                sockinfo->sockfd = -1;
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