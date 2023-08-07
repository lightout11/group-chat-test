#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include "inet_socket.h"

int inet_connect(char *ip, int port, int type)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int sfd = socket(addr.sin_family, type, IPPROTO_IP);
    if (sfd == -1)
    {
        perror("socket()");
        return -1;
    }

    if (connect(sfd, (const struct sockaddr *)&addr, addrlen) == -1)
    {
        perror("bind()");
        return -1;
    }

    return sfd;
}

int inet_bind(int port, int type)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int sfd = socket(addr.sin_family, type, IPPROTO_IP);
    if (sfd == -1)
    {
        perror("socket()");
        return -1;
    }

    if (bind(sfd, (const struct sockaddr *)&addr, addrlen) == -1)
    {
        perror("bind()");
        return -1;
    }

    return sfd;
}
    
int inet_listen(int port, int backlog)
{
    int sfd = inet_bind(port, SOCK_STREAM);
    if (sfd == -1)
    {
        return -1;
    }

    if (listen(sfd, backlog) == -1)
    {
        perror("listen()");
        return -1;
    }

    return sfd;
}