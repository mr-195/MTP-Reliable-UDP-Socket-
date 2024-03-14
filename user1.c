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

#include "msocket.h"

#define PORT_1 8000
#define PORT_2 8001

int main()
{
    int sockfd;

    if((sockfd = m_socket(AF_INET,SOCK_MTP,0)) < 0)
    {
        perror("msocket");
        exit(1);
    }
    struct sockaddr_in u1_addr, u2_addr;
    memset(&u1_addr, 0, sizeof(u1_addr));
    memset(&u2_addr, 0, sizeof(u2_addr));
    u1_addr.sin_family = AF_INET;
    u1_addr.sin_port = htons(PORT_1);
    u1_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    u2_addr.sin_family = AF_INET;
    u2_addr.sin_port = htons(PORT_2);
    u2_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(m_bind(sockfd, (struct sockaddr *)&u1_addr, sizeof(u1_addr)) < 0)
    {
        perror("m_bind");
        exit(1);
    }
    char msg[1024];
    memset(msg, 0, sizeof(msg));
    printf("Enter message: \n");
    scanf("%[^\n]s", msg);
    int msg_len = strlen(msg);
    for(int i = 0; i < msg_len; i++)
    {
        if(m_sendto(sockfd, msg + i, 1, 0, (struct sockaddr *)&u2_addr, sizeof(u2_addr)) < 0)
        {
            perror("m_sendto");
            exit(1);
        }
    }

    while(1);


}