#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "mysock.h"
#define PORT_1 8000
#define PORT_2 8001
#define MAX_MSG_LEN 100

int main()
{
    int sockfd;
    if ((sockfd = m_socket(AF_INET, SOCK_MTP, 0)) < 0)
    {
        perror("m_socket");
        exit(1);
    }
    struct sockaddr_in u1_addr, u2_addr;
    socklen_t u1_addr_len;
    memset(&u2_addr, 0, sizeof(u2_addr));
    u2_addr.sin_family = AF_INET;
    u2_addr.sin_port = htons(PORT_2);
    u2_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (m_bind(sockfd, "127.0.0.1", PORT_2, "127.0.0.1", PORT_1) < 0)
    {
        perror("m_bind");
        exit(1);
    }
    char msg[MAX_MSG_LEN];

    //    while(1)
    {
        u1_addr_len = sizeof(u1_addr);
        memset(msg, 0, sizeof(msg));
        int msg_len = m_recvfrom(sockfd, msg, MAX_MSG_LEN, 0, (struct sockaddr *)&u1_addr, &u1_addr_len);
        if (msg_len < 0)
        {
            perror("r_recvfrom");
            exit(1);
        }
        else
        {
            printf("%s", msg);
            fflush(stdout);
        }
    }
    // m_close(sockfd);
    return 0;
}