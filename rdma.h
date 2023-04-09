/*
 * RDMA helper functions and data structures
 */
#ifndef RDMA_H_
#define RDMA_H_

#include <infiniband/verbs.h>

#include "sokt.h"

/**
 * @brief RDMA context
 *
 */
struct rdma_context;

/**
 * @brief Open RDMA connection and modify the QPs to RTS
 *
 * @param is_primary
 * @param self_sockfd
 * @param others can be NULL
 * @param others_num
 * @param ht_addr
 * @param ht_size
 * @return struct rdma_context* NULL for failure
 */
struct rdma_context *rdma_open_connection(char is_primary, int self_sockfd, struct sokt_name_info **others, int others_num, void *ht_addr, size_t ht_size);

/**
 * @brief Close RDMA connection and release resources
 *
 * @param ctx
 * @param others_num
 */
void rdma_close_connection(struct rdma_context *ctx, int others_num);

/**
 * @brief Perform RDMA WRITE to all connected servers
 *
 * @param ctx
 * @param offset offset in memory
 * @param size size to be written
 * @param others_num
 * @return int -1 for failure
 */
int rdma_wrtie_all(const struct rdma_context *ctx, long offset, size_t size, int others_num);

/**
 * @brief Wait and poll for completion of RDMA WR from all connected servers
 *
 * @param ctx
 * @param others_num
 * @return int -1 for failure
 */
int rdma_wait_completion_all(const struct rdma_context *ctx, int others_num);

#endif