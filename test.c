#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mysock.h"
#define TYPE_SIZE sizeof(char)
#define SEQ_NUM_SIZE sizeof(int)
#define MAX_FRAME_SIZE 1024
int main()
{
    char msg [MAX_FRAME_SIZE];
    memset(msg, 0, sizeof(msg));
    printf("Enter message: \n");
    strcpy(msg, "D1324Hello");
    int i=1;
    int seq_num=0;
    char data[MAX_FRAME_SIZE];
    printf("Message: %c\n", msg[0]);
    data[0] = msg[0];
    for(int i=1;i<MAX_FRAME_SIZE;i++)
    {
        if(msg[i] >= '0' && msg[i] <= '9')
        {
            
            if((seq_num * 10 + (msg[i] - '0'))> 16)
            {
                break;
            }
          seq_num = seq_num * 10 + (msg[i] - '0');
        }
        data[i] = msg[i];
    }
    printf("Sequence number: %d\n", seq_num);
    printf("Data: %s\n", data);
    // send ACK message 
    char ack_msg[MAX_FRAME_SIZE];
    ack_msg[0] = 'A';
    // add the seqnum to the ack message
    char seq_num_str[100];
    sprintf(seq_num_str, "%d\0", seq_num);
    strcat(ack_msg, seq_num_str);
    printf("ACK message: %s\n", ack_msg);


}