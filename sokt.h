/*
 * Socket helper functions and data structures
 */
#ifndef SOKT_H_
#define SOKT_H_

#include <stddef.h>

/**
 * @brief Server address and port
 *
 */
struct sokt_name_info
{
    char *addr;
    char *port;
};

/**
 * @brief Code for sokt_message
 *
 */
enum sokt_message_code
{
    SOKT_CODE_PUT,
    SOKT_CODE_GET,
    SOKT_CODE_SUCCESS,
    SOKT_CODE_ERROR,
    SOKT_CODE_FULL,
    SOKT_CODE_NOT_FOUND
};

/**
 * @brief Socket message
 *
 */
struct sokt_message
{
    int key;
    int value;
    enum sokt_message_code code;
};

/**
 * @brief Show the content of a message
 *
 * @param msg
 */
void skot_message_show(const struct sokt_message *msg);

/**
 * @brief Setup socket connection on passive side
 *
 * @param addr for local, can be NULL
 * @param port for local
 * @return int -1 for failure
 */
int sokt_passive_open(char *addr, char *port);

/**
 * @brief Close socket connection on passive side
 *
 * @param sockfd
 */
void sokt_passive_close(int sockfd);

/**
 * @brief Setup socket connection on active side
 *
 * @param addr for remote
 * @param port for remote
 * @return int -1 for failure
 */
int sokt_active_open(char *addr, char *port);

/**
 * @brief Close socket connection on active side
 *
 * @param sockfd
 */
void sokt_active_close(int sockfd);

/**
 * @brief Setup socket connection after accept on passive side
 *
 * @param sockfd
 * @return int -1 for failure
 */
int sokt_passive_accept_open(int sockfd);

/**
 * @brief Close socket connection after accept on passive side
 *
 * @param connfd
 */
void sokt_passive_accept_close(int connfd);

/**
 * @brief Send a buf through a socket file descriptor
 *
 * @param sockfd
 * @param msg
 * @param size
 * @return int -1 for failure
 */
int sokt_send(int sockfd, char *msg, size_t size);

/**
 * @brief Receive a buf through a socket file descriptor
 *
 * @param sockfd
 * @param msg
 * @param size
 * @return int -1 for failure
 */
int sokt_recv(int sockfd, char *msg, size_t size);

#endif