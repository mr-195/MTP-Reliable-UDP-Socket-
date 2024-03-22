initmsocket.c

#include "msocket.h"
#include <pthread.h>
#include <sys/select.h>

#define P(s) semop(s, &pop, 1)
#define V(s) semop(s, &vop, 1)

void display(){
    key_t shm_key = ftok(".", KEY_SM);
    int shm_id = shmget(shm_key, sizeof(SharedMemory)*25, IPC_CREAT | 0777);
    SharedMemory *shared_memory = (SharedMemory *)shmat(shm_id, NULL, 0);

    for(int i=0;i<25;i++){
        if(shared_memory[i].is_allocated==2){
            printf("UDP ID in display(): %d\n", shared_memory[i].udp_socket_id);
            printf("Destination port in display(): %d\n", shared_memory[i].dest_addr.sin_port);
        }
    }
}

/* The signal handler for the child process */
void signalHandler ( int sig )
{
    if (sig == SIGINT) {
        // key for global shared memory
        key_t shm_key = ftok(".", KEY_SM);
        if (shm_key == -1) {
            perror("Error creating shared memory key");
            exit(EXIT_FAILURE);
        }

        // Create a shared memory segment
        int shm_id = shmget(shm_key, sizeof(SharedMemory)*25, IPC_CREAT | 0666);
        if (shm_id == -1) {
            perror("Error creating shared memory segment\n");
            exit(EXIT_FAILURE);
        }

        // Delete the shared memory segment (optional)
        shmctl(shm_id, IPC_RMID, NULL);

        printf("Detached shared memory\n");

        int key1 = ftok(".", 15);
        int mtx = semget(key1, 1, 0777);
        semctl(mtx, 0, IPC_RMID, 0);

        int key2 = ftok(".", 16);
        int mtx_while = semget(key2, 1, 0777);
        semctl(mtx_while, 0, IPC_RMID, 0);


        int key3 = ftok(".", 17);
        int csmutex = semget(key3, 1, 0777);
        semctl(csmutex, 0, IPC_RMID, 0);

        int key4 = ftok(".", 18);
        int mtx_bind = semget(key4, 1, 0777);
        semctl(mtx_bind, 0, IPC_RMID, 0);

        printf("Detached mutexes\n");

        exit(0);
    }
}

int send_message(int sockfd, struct sockaddr_in *dest_addr, const char *message) {
    // Send the message
    ssize_t bytes_sent = sendto(sockfd, message, strlen(message), 0,
                                (struct sockaddr *)dest_addr, sizeof(struct sockaddr_in));
    if (bytes_sent < 0) {
        perror("Error sending message");
        return -1;
    }

    return 0;
}


// void set_null(char *arr,int len){
//     for(int i=0;i<len;i++){
//         arr[i] = '\0';
//     }
// }

int min(int a,int b){
    return (a<b)?a:b;
}
void dis_arr(int *a,int b){
    printf(": ");
    for(int i=0;i<b;i++) printf("%d ",a[i]);
    printf("\n");
}

void *thread_R(void *arg) {
    int key3 = ftok(".", 17);
    int csmutex = semget(key3, 1, 0777|IPC_CREAT);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;


    // Get the shared memory pointer from the argument
    SharedMemory *shared_memory = (SharedMemory *)arg;
    fd_set readfds;
   
    while(1) {
        sleep(1);
        FD_ZERO(&readfds);
        for(int i=0; i<MAX_MTP_SOCKETS; i++) {
            if (shared_memory[i].is_allocated == 2) {
                FD_SET(shared_memory[i].udp_socket_id, &readfds);
            }
        }

        // Set the timeout duration
        struct timeval timeout;
        timeout.tv_sec = 2;

        // Wait for activity on any of the sockets
        if (select(FD_SETSIZE, &readfds, NULL, NULL, &timeout) == -1) {
            perror("Select error");
            exit(EXIT_FAILURE);
        }

        for(int i=0; i<MAX_MTP_SOCKETS; i++) {
            if(FD_ISSET(shared_memory[i].udp_socket_id, &readfds)) {
                // struct sockaddr_in src_addr = shared_memory[i].source_addr;
                socklen_t sockfd = shared_memory[i].udp_socket_id;
                char recv_msg[MAX_MESSAGE_SIZE];
                set_null(recv_msg, MAX_MESSAGE_SIZE);
                recvfrom(sockfd, recv_msg, MAX_MESSAGE_SIZE, 0, NULL, NULL);
                size_t msg_len = strlen(recv_msg);

                int seq_no=-1;

                // i=0 er socket ta tahole receive korche kibhabe
                // i=1 ta toh receiver

                printf("Recv msg at thread r : %s\n",recv_msg);
                if(strlen(recv_msg)==0) printf("yepp\n");

                // printf("length at r : %d\n",strlen(recv_msg));
                // printf("length at rr : %c\n",recv_msg[msg_len-1]);
                // printf("length at rrr : %c\n",recv_msg[msg_len-2]);
                // printf("length at rrrr : %c\n",recv_msg[msg_len-3]);
                // printf("length at rrrrr : %c\n",recv_msg[msg_len-4]);

                P(csmutex);
                if(recv_msg[msg_len-4] == '-'){
                    // single digit seq no
                    char ch = recv_msg[msg_len-3];
                    seq_no=ch-48;
                   
                }
                else if(recv_msg[msg_len-5] == '-'){
                    // double digit seq no
                    char ch1 = recv_msg[msg_len-3];
                    char ch2 = recv_msg[msg_len-4];
                    seq_no=(ch2-48)*10+ch1-48;
                }
                V(csmutex);

                printf("seq no at thread r : %d\n",seq_no);

                if(recv_msg[msg_len-1] == '1'){
                    // then it is a DATA msg
                    // DATA-10-1
                    // DATA-9-1
                    // recv_msg = DATA-10 , DATA-seq_no    
                    P(csmutex);
                    recv_msg[msg_len-1] = '\0'; msg_len--;
                    recv_msg[msg_len-1] = '\0'; msg_len--;              

                    int *arr1 = shared_memory[i].rwnd.sequence_numbers ;


                    // printf(";; ");
                    // dis_arr(shared_memory[i].rwnd.sequence_numbers,MAX_RWND);
                    // V(csmutex);

                    // P(csmutex);

                    int p1 = arr1[0];
                    int k;
                    for(k = 0;k<MAX_RWND;k++){
                        if(arr1[k]==seq_no) break;
                    }

                    // V(csmutex);

                    int g=0;
                    int u1=0;
                    if(arr1[k]==-1){
                        // duplicate DATA-seq_no received
                        printf("Duplicate data %d received \n",seq_no);
                        g=1;
                    }
                    else {
                        // write recv_msg=DATA-seq_no in the receive buffer
                       
                        for (int w = 0; w < MAX_RECV_BUF_SZ; w++)
                        {
                            if(strlen(shared_memory[i].recv_buffer[w]) == 0){
                                // then this place in the receive is empty
                                strcpy(shared_memory[i].recv_buffer[w],recv_msg);
                                printf("wr this in shm : %s\n",recv_msg);
                                u1 = 1;
                                break;
                            }
                        }
                        if(u1==1) arr1[k] = -1;
                        if(u1==1) shared_memory[0].send_max--;
                    }

                    // P(csmutex);
                    printf(";; ");
                    dis_arr(shared_memory[i].rwnd.sequence_numbers,MAX_RWND);
                    // V(csmutex);


                    int u=0;
                    for (int w = 0; w < MAX_RECV_BUF_SZ; w++)
                    {
                        if(strlen(shared_memory[i].recv_buffer[w]) == 0){
                            u++;
                        }
                    }

                    shared_memory[0].send_max = u;

                   
                    //
                    if(g==0 && u1==1){
                        // not a duplicate message
                        // send ACK-U-seq_no-0
                        struct sockaddr_in dest_addr = shared_memory[i].dest_addr;
                        char message[MAX_MESSAGE_SIZE];
                        set_null(message, MAX_MESSAGE_SIZE);
                        sprintf(message, "ACK-%d-%d-0", u, seq_no);
                        int status = send_message(sockfd, &dest_addr, message);
                        if (status != -1) {
                            printf("Ack sent @ %s\n" , message);
                        }
                        sleep(2);
                    }

                                       

                    if(arr1[0]==-1){
                        // P(csmutex);
                        // update receiver window
                        printf("update receiver window\n");
                        int cnt=0;
                        for(int h=0;h<MAX_RWND;h++){
                            if(arr1[h]==-1) cnt++; else break;
                        }
                        printf("p1 = %d\n",p1);
                        p1 = (p1+cnt)%16;

                        int cnt1 = 0;
                        for(int k=cnt; k<MAX_RWND; k++){
                            if(arr1[k]>=0) cnt1++;
                        }

                        int h;
                        int yyt[MAX_RWND];
                        for(h=0;h<MAX_RWND;h++){
                            yyt[h] = p1;
                            p1 = (p1+1)%16;
                            // shared_memory[i].last_entry_rwnd = (arr1[h]+1)%16;
                        }

                        for(h=cnt;h<MAX_RWND;h++){
                            arr1[h-cnt] = arr1[h];
                        }
                        for(h=MAX_RWND-cnt;h<MAX_RWND;h++){
                            arr1[h] = -2;
                        }
                        for(h=0;h<MAX_RWND;h++){
                            if(arr1[h]!=-1) arr1[h] = yyt[h];
                        }

                        // shared_memory[i].rwnd.size = MAX_RWND-cnt;

                        // int u2 = u-cnt1;
                        // for(int y=MAX_RWND-cnt;y<MAX_RWND;y++) arr1[y]=-2;
                        // for(int y=MAX_RWND-cnt;y<MAX_RWND;y++) {
                        //     if(u2>0){
                        //         arr1[y] = p1;
                        //         shared_memory[i].rwnd.size++;
                        //         u2--;
                        //         p1 = (p1+1)%16;
                        //     }
                        // }

                        // for(int y=0;y<MAX_RWND;y++){
                        //     if(arr1[y]>0){
                        //         shared_memory[i].last_entry_rwnd = arr1[y];
                        //     }
                        // }

                        // shared_memory[i].last_entry_rwnd = (shared_memory[i].last_entry_rwnd+1)%16;

                        // shared_memory[i].rwnd.size = min(MAX_RWND,u);

                        printf("rwdd ");
                        dis_arr(arr1,MAX_RWND);
                    }

                    else {
                        // no need to update receiver window
                    }

                    V(csmutex);

                   
                   
                }
               
                else {
                    // it is ACK msg
                    recv_msg[msg_len-1] = '\0'; msg_len--;
                    recv_msg[msg_len-1] = '\0'; msg_len--;

                    // recv_msg == ACK-10 , ACK-U-seq_no
                    // send_max = U

                    // extract U
                    P(csmutex);
                    for(int t=msg_len-1;t>=0;t--){
                        if(recv_msg[t]=='-'){
                            shared_memory[0].send_max = recv_msg[t-1]-48;
                            break;
                        }
                    }
                    // V(csmutex);

                    // oi -1 er case ta send_size1

                    // P(csmutex);
                    int *arr = shared_memory[i].swnd.sequence_numbers;
                    int sz1 = shared_memory[i].swnd.size;
                    int uwu=0;
                    for(int j=0;j<MAX_SWND;j++){
                        if(arr[j]==seq_no){
                            // uwu=1;
                            arr[j]=-1;
                             

                            if(arr[0]==-1){
                                // update swnd window
                               
                                printf("update swnd window i = %d\n",i);

                                // printf("swdd2 ");
                                // dis_arr(arr,MAX_SWND);

                                int cnt=0;
                                int j1;
                                for(j1=0;j1<MAX_SWND;j1++){
                                    if(arr[j1]==-1){cnt++;}
                                    else break;
                                }
                                int q = sz1-cnt;
                                for(int p=j1;p<sz1;p++){
                                    arr[p-j1] = arr[p];
                                }

                                // printf("swdd1 ");
                                // dis_arr(arr,MAX_SWND);


                                for(int y=q;y<MAX_SWND;y++) arr[y] = -2;
                                shared_memory[i].swnd.size = q;

                                // update send buffer
                                for(int y=shared_memory[i].send_ptr11;y<shared_memory[i].send_ptr11+cnt;y++){
                                    set_null(shared_memory[i].send_buffer[y%MAX_SEND_BUF_SZ],MAX_MESSAGE_SIZE);
                                }


                                shared_memory[i].send_ptr11 = (shared_memory[i].send_ptr11+cnt)%MAX_SEND_BUF_SZ;
                                printf("sendsize1 @updt swnd : %d @%ld\n", shared_memory[i].send_size1,time(NULL)-1711100847);
                                // shared_memory[i].send_size1 -= cnt;

                                int kk = 0;
                                for(int s=0;s<MAX_SEND_BUF_SZ;s++){
                                    if(strlen(shared_memory[i].send_buffer[s]) != 0) kk++;
                                }
                                shared_memory[i].send_size1 = kk;


                                printf("sendsize1 @updt swnd1 : %d @%ld\n", shared_memory[i].send_size1,time(NULL)-1711100847);
                                printf("swdd ");
                                dis_arr(arr,MAX_SWND);
                               
                            }


                            break;
                        }


                    }
                   
                    V(csmutex);

                   
                }
                // printf("%s\n", recv_msg);
            }
        }
    }

    return NULL;
}

void *thread_S(void *arg) {
    int key3 = ftok(".", 17);
    int csmutex = semget(key3, 1, 0777|IPC_CREAT);
    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    // Get the shared memory pointer from the argument
    SharedMemory *shared_memory = (SharedMemory *)arg;

    while(1) {
        sleep(1);
        for(int i=0; i<MAX_MTP_SOCKETS; i++) {
            if(shared_memory[i].is_allocated != 2) continue;
            socklen_t sockfd = shared_memory[i].udp_socket_id;
            struct sockaddr_in dest_addr = shared_memory[i].dest_addr;
            if (time(NULL) - shared_memory[i].timestamp >= 10 && shared_memory[i].swnd.size>0 && shared_memory[i].send_max>0) {
                P(csmutex);
                int jj = shared_memory[i].send_max;
                int pos = shared_memory[i].send_ptr11;
                printf("Here waiting %d %d %d\n",shared_memory[i].swnd.size, pos,i);
                for(int j=pos; j<pos + shared_memory[i].swnd.size; j++) {
                    if(jj==0) break;
                    jj--;
                    char message[MAX_MESSAGE_SIZE];
                    set_null(message,MAX_MESSAGE_SIZE);
                    strcpy(message, shared_memory[i].send_buffer[j]);

                    int ff = shared_memory[i].swnd.sequence_numbers[0];
                    if(ff==-2) break;
                    int AG = (ff+j-pos)%16 ;
                    sprintf(message, "%s-%d-1", message, AG);
                    int status = send_message(sockfd, &dest_addr, message);
                    printf("tt %d\n",AG);
                    if(status!=-1){
                        // P(csmutex);
                        shared_memory[i].timestamp = time(NULL);
                        printf("timeout sent\n");
                        // V(csmutex);
                    }
                }
                V(csmutex);
            }
        }

        for(int i=0; i<MAX_MTP_SOCKETS; i++) {
            if(shared_memory[i].is_allocated != 2) continue;
            if(shared_memory[i].send_size1 == 0) continue;
            socklen_t sockfd = shared_memory[i].udp_socket_id;
            printf("swndsize : %d \n",shared_memory[i].swnd.size);
            printf("sendsize : %d ",shared_memory[i].send_size);
            printf("sendmax : %d\n",shared_memory[i].send_max);
            while(shared_memory[i].swnd.size != 5 && shared_memory[i].send_size!=0 && shared_memory[i].send_max>0) {
                struct sockaddr_in dest_addr = shared_memory[i].dest_addr;
                char message[MAX_MESSAGE_SIZE];
                set_null(message,MAX_MESSAGE_SIZE);

                P(csmutex);

                strcpy(message, shared_memory[i].send_buffer[shared_memory[i].send_ptr1]);
                // printf("Msg len1 @th S : %d\n",strlen(message));
                sprintf(message, "%s-%d-1", message, shared_memory[i].next_seq_no);
                printf("Msg to be sent @th S : %s\n",message);
                // printf("Msg len @th S : %d\n",strlen(message));


                // printf("yoo\n");
                int status = send_message(sockfd, &dest_addr, message);

                V(csmutex);
                if (status != -1) {
                    P(csmutex);
                    shared_memory[i].send_size--;
                    shared_memory[i].swnd.size++;
                    int last_seq;
                    last_seq = shared_memory[i].next_seq_no;
                    shared_memory[i].swnd.sequence_numbers[shared_memory[i].swnd.size-1] = last_seq;
                    shared_memory[i].next_seq_no = (shared_memory[i].next_seq_no+1)%16;
                    shared_memory[i].send_ptr1 = (shared_memory[i].send_ptr1+1)%MAX_SEND_BUF_SZ;
                    // shared_memory[i].send_max--;
                    printf("Msg sent successfully\n");
                    printf("swd @thSS "); dis_arr(shared_memory[i].swnd.sequence_numbers,MAX_SWND);
                    shared_memory[i].timestamp = time(NULL);
                    V(csmutex);
                }
                sleep(2);
            }
        }
    }

    return NULL;
}

void *garbage_collector(void *arg) {
    // Retrieve the shared memory key
    key_t shm_key = *((key_t *)arg);

    // Attach to the shared memory segment
    int shm_id = shmget(shm_key, sizeof(SharedMemory), 0666);
    if (shm_id == -1) {
        perror("Error attaching to shared memory");
        exit(EXIT_FAILURE);
    }

    // Get a pointer to the shared memory segment
    SharedMemory *shared_memory = (SharedMemory *)shmat(shm_id, NULL, 0);
    if (shared_memory == (void *)-1) {
        perror("Error getting pointer to shared memory");
        exit(EXIT_FAILURE);
    }

    // Check if any MTP sockets need to be deallocated
    // For simplicity, assume a timeout mechanism or some other condition for deallocation

    // Detach from the shared memory segment
    if (shmdt(shared_memory) == -1) {
        perror("Error detaching from shared memory");
        exit(EXIT_FAILURE);
    }

    return NULL;
}

int main() {
    signal(SIGINT, signalHandler);

    // key for global shared memory
    key_t shm_key = ftok(".", KEY_SM);
    if (shm_key == -1) {
        perror("Error creating shared memory key");
        exit(EXIT_FAILURE);
    }

    // Create a shared memory segment
    int shm_id = shmget(shm_key, sizeof(SharedMemory)*25, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("Error creating shared memory segment");
        exit(EXIT_FAILURE);
    }

    // shared memory array
    SharedMemory *shared_memory = (SharedMemory *)shmat(shm_id, NULL, 0);

    for(int i=0;i<25;i++){
        shared_memory[i].is_allocated = 0;
    }

    int key3 = ftok(".", 17);
    int csmutex = semget(key3, 1, 0777| IPC_CREAT);
    semctl(csmutex, 0, SETVAL, 1);

    // Create threads R and S
    pthread_t thread_R_id, thread_S_id;
    if (pthread_create(&thread_R_id, NULL, thread_R, shared_memory) != 0) {
        perror("Error creating thread R");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&thread_S_id, NULL, thread_S, shared_memory) != 0) {
        perror("Error creating thread S");
        exit(EXIT_FAILURE);
    }

    // Start garbage collector process G
    // pthread_t garbage_collector_id;
    // if (pthread_create(&garbage_collector_id, NULL, garbage_collector, &shm_key) != 0) {
    //     perror("Error creating garbage collector");
    //     exit(EXIT_FAILURE);
    // }


    struct sembuf pop, vop;
    pop.sem_num = vop.sem_num = 0;
    pop.sem_flg = vop.sem_flg = 0;
    pop.sem_op = -1;
    vop.sem_op = 1;

    int key1 = ftok(".", 15);
    int mtx = semget(key1, 1, 0777|IPC_CREAT);
    semctl(mtx, 0, SETVAL, 0);

    int key2 = ftok(".", 16);
    int mtx_while = semget(key2, 1, 0777|IPC_CREAT);
    semctl(mtx_while, 0, SETVAL, 0);


   

    int key4 = ftok(".", 18);
    int mtx_bind = semget(key4, 1, 0777|IPC_CREAT);
    semctl(mtx_bind, 0, SETVAL, 0);

    while(1){
        // P(mtx_while);
        // display();
        P(mtx);
        printf("New Socket to be created \n");

        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        int i1;

        for(int i=0;i<25;i++){
            if(shared_memory[i].is_allocated == 0){
                P(csmutex);
                shared_memory[i].is_allocated = 1;
                shared_memory[i].udp_socket_id = sockfd;
                printf("Udp socket id in init: %d \n" , shared_memory[i].udp_socket_id);
                i1 =i;
                V(csmutex);



                break;
            }
        }

        V(mtx_while); // to signal m_bind()

        printf("Signal sent to bind\n");

        P(mtx_bind) ; // wait for signal from bind

       

        if ( bind(sockfd, (struct sockaddr *)&shared_memory[i1].source_addr, sizeof(shared_memory[i1].source_addr)) < 0 ) {
            perror("bind failed in init");
            exit(EXIT_FAILURE);
        }

        P(csmutex);
        shared_memory[i1].send_ptr1 = 0;
        shared_memory[i1].send_ptr11 = 0;
        shared_memory[i1].last_entry_rwnd = 5;
        shared_memory[i1].send_ptr2 = 0;
        shared_memory[i1].send_size = 0;
        shared_memory[i1].send_size1 = 0;
        shared_memory[i1].last_seq_no = 0;
        shared_memory[i1].rwnd.size = MAX_RWND;
        shared_memory[i1].swnd.size = 0;
        shared_memory[i1].send_max = MAX_RWND;
        shared_memory[i1].next_seq_no = 0;

        for(int i=0;i<MAX_RWND;i++){
            shared_memory[i1].rwnd.sequence_numbers[i] = i;
        }
        for(int i=0;i<MAX_SWND;i++){
            shared_memory[i1].swnd.sequence_numbers[i] = -2;
        }

        for(int i=0;i<MAX_RECV_BUF_SZ;i++){
            set_null(shared_memory[i1].recv_buffer[i],MAX_MESSAGE_SIZE);
        }
        for(int i=0;i<MAX_SEND_BUF_SZ;i++){
            set_null(shared_memory[i1].send_buffer[i],MAX_MESSAGE_SIZE);
        }
        V(csmutex);

        printf("Bind Done Successfully\n") ;

        // V(mtx_while);
    }

    // Wait for threads and garbage collector to finish
    pthread_join(thread_R_id, NULL);
    pthread_join(thread_S_id, NULL);
    // pthread_join(garbage_collector_id, NULL);

    // Detach from the shared memory segment
    if (shmdt(shared_memory) == -1) {
        perror("Error detaching from shared memory");
        exit(EXIT_FAILURE);
    }

    // Delete the shared memory segment (optional)
    shmctl(shm_id, IPC_RMID, NULL);

    return 0;
}