#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sokt.h"

#define BACKLOG 5

struct addrinfo *getaddrinfo_wrapper(char *addr, char *port);
void close_wrapper(int sockfd);

void skot_message_show(const struct sokt_message *msg)
{
    printf("code: ");

    switch (msg->code)
    {
    case SOKT_CODE_PUT:
        printf("PUT       ");
        break;
    case SOKT_CODE_GET:
        printf("GET       ");
        break;
    case SOKT_CODE_SUCCESS:
        printf("SUCCESS   ");
        break;
    case SOKT_CODE_ERROR:
        printf("ERROR     ");
        break;
    case SOKT_CODE_FULL:
        printf("FULL      ");
        break;
    case SOKT_CODE_NOT_FOUND:
        printf("NOT_FOUND ");
        break;
    default:
        printf("unknown  ");
        break;
    }

    printf("key: %-8d value: %d\n", msg->key, msg->value);
}

int sokt_passive_open(char *addr, char *port)
{
    assert(port);

    int sockfd = -1;

    struct addrinfo *servinfo = getaddrinfo_wrapper(addr, port);
    if (!servinfo)
    {
        fprintf(stderr, "getaddrinfo_wrapper failed\n");
        goto out1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
                    servinfo->ai_protocol);
    if (sockfd == -1)
    {
        perror("socket");
        goto out2;
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        perror("bind");
        goto out3;
    }

    if (listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        goto out3;
    }

    goto out2;

out3:
    sokt_passive_close(sockfd);
    sockfd = -1;

out2:
    freeaddrinfo(servinfo);

out1:
    return sockfd;
}

void sokt_passive_close(int sockfd)
{
    close_wrapper(sockfd);
}

int sokt_active_open(char *addr, char *port)
{
    assert(addr);
    assert(port);

    int sockfd = -1;

    struct addrinfo *servinfo = getaddrinfo_wrapper(addr, port);
    if (!servinfo)
    {
        fprintf(stderr, "getaddrinfo_wrapper failed\n");
        goto out1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
                    servinfo->ai_protocol);
    if (sockfd == -1)
    {
        perror("socket");
        goto out2;
    }

    if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        perror("connect");
        goto out3;
    }

    goto out2;

out3:
    sokt_active_close(sockfd);
    sockfd = -1;

out2:
    freeaddrinfo(servinfo);

out1:
    return sockfd;
}

void sokt_active_close(int sockfd)
{
    close_wrapper(sockfd);
}

int sokt_passive_accept_open(int sockfd)
{
    int connfd = accept(sockfd, NULL, NULL);
    if (connfd == -1)
    {
        perror("accept");
        return -1;
    }

    return connfd;
}

void sokt_passive_accept_close(int connfd)
{
    close_wrapper(connfd);
}

int sokt_send(int sockfd, char *msg, size_t size)
{
    unsigned sent = 0;

    while (sent < size)
    {
        ssize_t step = write(sockfd, msg + sent, size - sent);
        if (step == -1)
        {
            perror("write");
            return -1;
        }

        sent += step;
    }

    return 0;
}

int sokt_recv(int sockfd, char *msg, size_t size)
{
    unsigned recv = 0;

    while (recv < size)
    {
        ssize_t step = read(sockfd, msg + recv, size - recv);
        if (step == -1)
        {
            perror("read");
            return -1;
        }

        recv += step;
    }

    return 0;
}

struct addrinfo *getaddrinfo_wrapper(char *addr, char *port)
{
    assert(port);

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(addr, port, &hints, &servinfo) != 0)
    {
        perror("getaddrinfo");
        return NULL;
    }

    return servinfo;
}

void close_wrapper(int sockfd)
{
    if (close(sockfd) == -1)
    {
        perror("close");
    }
}