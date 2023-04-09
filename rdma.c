#include <assert.h>
#include <infiniband/verbs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "parameters.h"
#include "rdma.h"
#include "sokt.h"

#define COUNT 1
#define IB_PORT 1

struct QP_info
{
    int lid;
    int qpn;
    int psn;

    uint64_t addr;
    uint32_t rkey;
};

int connect_with_backup(struct rdma_context *ctx, int sockfd, int index);
int connect_with_primary(struct rdma_context *ctx, struct sokt_name_info **others);
int connect_between_qps(struct rdma_context *ctx, int index);

struct rdma_context
{
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq **cq;
    struct ibv_qp **qp;

    struct QP_info *local_qp_info;
    struct QP_info *remote_qp_info;
};

struct rdma_context *rdma_open_connection(char is_primary, int self_sockfd, struct sokt_name_info **others, int others_num, void *ht_addr, size_t ht_size)
{
    assert(self_sockfd != -1);
    assert(others_num >= 0);
    assert(ht_addr);
    assert(ht_size);

    srand48(getpid() * time(NULL));
    struct rdma_context *ctx;
    struct ibv_device **dev_list, *ib_dev;

    dev_list = ibv_get_device_list(NULL);
    if (!dev_list)
    {
        perror("ibv_get_device_list");
        goto out1;
    }
    ib_dev = *dev_list;
    if (!ib_dev)
    {
        fprintf(stderr, "failed to find IB device\n");
        goto out2;
    }

    ctx = calloc(1, sizeof(struct rdma_context));
    if (!ctx)
    {
        perror("calloc for ctx");
        goto out2;
    }

    ctx->ctx = ibv_open_device(ib_dev);
    if (!ctx->ctx)
    {
        perror("ibv_open_device");
        goto out3;
    }

    ctx->pd = ibv_alloc_pd(ctx->ctx);
    if (!ctx->pd)
    {
        perror("ibv_alloc_pd");
        goto out3;
    }

    ctx->mr = ibv_reg_mr(ctx->pd, ht_addr, ht_size,
                         IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
    if (!ctx->mr)
    {
        perror("ibv_reg_mr");
        goto out3;
    }

    ctx->cq = calloc(others_num, sizeof(struct ibv_cq *));
    if (!ctx->cq)
    {
        perror("calloc for ctx->cq");
        goto out3;
    }
    for (int i = 0; i < others_num; i++)
    {
        ctx->cq[i] = ibv_create_cq(ctx->ctx, COUNT * others_num * 2, NULL, NULL, 0);
        if (!ctx->cq[i])
        {
            perror("ibv_create_cq");
            goto out3;
        }
    }

    ctx->qp = calloc(others_num, sizeof(struct ibv_qp *));
    if (!ctx->qp)
    {
        perror("calloc for ctx->qp");
        goto out3;
    }
    for (int i = 0; i < others_num; i++)
    {
        struct ibv_qp_init_attr qp_init_attr = {
            .send_cq = ctx->cq[i],
            .recv_cq = ctx->cq[i],
            .cap = {
                .max_send_wr = COUNT * others_num,
                .max_recv_wr = COUNT,
                .max_send_sge = 1,
                .max_recv_sge = 1,
            },
            .qp_type = IBV_QPT_RC,
        };

        ctx->qp[i] = ibv_create_qp(ctx->pd, &qp_init_attr);
        if (!ctx->qp[i])
        {
            perror("ibv_create_qp");
            goto out3;
        }

        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_INIT,
            .pkey_index = 0,
            .port_num = IB_PORT,
            .qp_access_flags = IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE,
        };
        int init_flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;

        if (ibv_modify_qp(ctx->qp[i], &qp_attr, init_flags))
        {
            fprintf(stderr, "failed to modify QP %d to INIT\n", i);
            goto out3;
        }
    }

    // Get information for RDMA connection
    ctx->local_qp_info = calloc(others_num, sizeof(struct QP_info));
    if (!ctx->local_qp_info)
    {
        perror("calloc for ctx->local_qp_info");
        goto out3;
    }

    struct ibv_port_attr port_attr;
    if (ibv_query_port(ctx->ctx, IB_PORT, &port_attr))
    {
        perror("ibv_query_port");
        goto out3;
    }

    for (int i = 0; i < others_num; i++)
    {
        ctx->local_qp_info[i].lid = port_attr.lid;
        if (port_attr.link_layer == IBV_LINK_LAYER_INFINIBAND && !ctx->local_qp_info[i].lid)
        {
            fprintf(stderr, "faild to get LID for QP %d\n", i);
            goto out3;
        }
        ctx->local_qp_info[i].qpn = ctx->qp[i]->qp_num;
        ctx->local_qp_info[i].psn = lrand48() & 0xffffff;
        ctx->local_qp_info[i].addr = (uint64_t)ht_addr;
        ctx->local_qp_info[i].rkey = ctx->mr->rkey;
    }

    ctx->remote_qp_info = calloc(others_num, sizeof(struct QP_info));
    if (!ctx->remote_qp_info)
    {
        perror("calloc for ctx->remote_qp_info");
        goto out3;
    }

    if (is_primary == 1)
    {
        for (int i = 0; i < others_num; i++)
        {
            if (connect_with_backup(ctx, self_sockfd, i))
            {
                fprintf(stderr, "failed to connect with backups %d and modify QP to RTS\n", i);
                goto out3;
            }
        }
    }
    else
    {
        if (connect_with_primary(ctx, others))
        {
            fprintf(stderr, "failed to connect with primary and modify QP to RTS\n");
            goto out3;
        }
    }

    goto out2;

out3:
    rdma_close_connection(ctx, others_num);
    ctx = NULL;

out2:
    ibv_free_device_list(dev_list);

out1:
    return ctx;
}

void rdma_close_connection(struct rdma_context *ctx, int others_num)
{
    if (!ctx)
    {
        return;
    }

    if (ctx->remote_qp_info)
    {
        free(ctx->remote_qp_info);
    }

    if (ctx->local_qp_info)
    {
        free(ctx->local_qp_info);
    }

    if (ctx->qp)
    {
        for (int i = 0; i < others_num; i++)
        {
            if (ctx->qp[i])
            {
                ibv_destroy_qp(ctx->qp[i]);
            }
        }

        free(ctx->qp);
    }

    if (ctx->cq)
    {
        for (int i = 0; i < others_num; i++)
        {
            if (ctx->cq[i])
            {
                ibv_destroy_cq(ctx->cq[i]);
            }
        }

        free(ctx->cq);
    }

    if (ctx->mr)
    {
        ibv_dereg_mr(ctx->mr);
    }

    if (ctx->pd)
    {
        ibv_dealloc_pd(ctx->pd);
    }

    if (ctx->ctx)
    {
        ibv_close_device(ctx->ctx);
    }

    free(ctx);
}

int rdma_wrtie_all(const struct rdma_context *ctx, long offset, size_t size, int others_num)
{
    struct ibv_sge list = {
        .addr = ctx->local_qp_info[0].addr + offset, // ctx->local_qp_info[i].addr are the same
        .length = size,
        .lkey = ctx->mr->lkey};

#ifdef LOG
    printf("local_addr    :\t%ld offset: %-8ld local_real_addr : %ld\n", ctx->local_qp_info[0].addr, offset, list.addr);
#endif

    struct ibv_send_wr *wr = calloc(others_num, sizeof(struct ibv_send_wr));
    if (!wr)
    {
        perror("calloc for wr");
        return -1;
    }

    for (int i = 0; i < others_num; i++)
    {
        struct ibv_send_wr *bad_wr;
        wr[i].sg_list = &list;
        wr[i].num_sge = 1;
        wr[i].opcode = IBV_WR_RDMA_WRITE;
        wr[i].send_flags = IBV_SEND_SIGNALED;
        wr[i].wr.rdma.remote_addr = ctx->remote_qp_info[i].addr + offset;
        wr[i].wr.rdma.rkey = ctx->remote_qp_info[i].rkey;
        wr[i].next = NULL;

        if (ibv_post_send(ctx->qp[i], &wr[i], &bad_wr) != 0)
        {
            perror("ibv_post_send");
            return -1;
        }

#ifdef LOG
        printf("remote_addr %2d:\t%ld offset: %-8ld remote_real_addr: %ld\n",
               i, ctx->remote_qp_info[i].addr, offset, wr[i].wr.rdma.remote_addr);
#endif
    }

    return 0;
}

int rdma_wait_completion_all(const struct rdma_context *ctx, int others_num)
{
    for (int i = 0; i < others_num; i++)
    {
        struct ibv_wc wc[COUNT];
        int n;

        do
        {
            n = ibv_poll_cq(ctx->cq[i], others_num, wc);

            if (n < 0)
            {
                fprintf(stderr, "ibv_poll_cq\n");
                return -1;
            }
        } while (n < 1);

        for (int i = 0; i < n; i++)
        {
            if (wc[i].status != IBV_WC_SUCCESS)
            {
                fprintf(stderr, "failed ibv_poll_cq status %s\n",
                        ibv_wc_status_str(wc[i].status));
                return -1;
            }
        }
    }

    return 0;
}

int connect_with_backup(struct rdma_context *ctx, int sockfd, int index)
{
    assert(ctx);
    assert(sockfd != -1);

    int rv = -1;

    int connfd = sokt_passive_accept_open(sockfd);
    if (connfd == -1)
    {
        fprintf(stderr, "sokt_passive_accept_open failed\n");
        goto out1;
    }

    char *buf = (char *)calloc(1, sizeof(struct QP_info));
    if (!buf)
    {
        perror("calloc for buf");
        goto out2;
    }

    // Get remote IB information
    if (sokt_recv(connfd, buf, sizeof(struct QP_info)) == -1)
    {
        fprintf(stderr, "sokt_recv failed\n");
        goto out3;
    }
    memcpy(&(ctx->remote_qp_info[index]), buf, sizeof(struct QP_info));
    printf("remote %d\tlid: %d qpn: %-4d psn: %-9d addr: %ld rkey: %u\n",
           index, ctx->remote_qp_info[index].lid, ctx->remote_qp_info[index].qpn, ctx->remote_qp_info[index].psn,
           ctx->remote_qp_info[index].addr, ctx->remote_qp_info[index].rkey);

    // Setup RMDA connection
    if (connect_between_qps(ctx, index))
    {
        fprintf(stderr, "connect_between_qps failed for QP %d\n", index);
        goto out3;
    }

    // Send local IB information
    memcpy(buf, &(ctx->local_qp_info[index]), sizeof(struct QP_info));
    if (sokt_send(connfd, buf, sizeof(struct QP_info)) == -1)
    {
        fprintf(stderr, "sokt_send failed\n");
        goto out3;
    }
    printf("local  %d\tlid: %d qpn: %-4d psn: %-9d addr: %ld rkey: %u\n",
           index, ctx->local_qp_info[index].lid, ctx->local_qp_info[index].qpn, ctx->local_qp_info[index].psn,
           ctx->local_qp_info[index].addr, ctx->local_qp_info[index].rkey);

    rv = 0;

out3:
    free(buf);

out2:
    sokt_passive_accept_close(connfd);

out1:
    return rv;
}

int connect_with_primary(struct rdma_context *ctx, struct sokt_name_info **others)
{
    assert(ctx);
    assert(others);

    int rv = -1;

    int sockfd = sokt_active_open(others[0]->addr, others[0]->port);
    if (sockfd == -1)
    {
        fprintf(stderr, "sokt_active_open failed\n");
        goto out1;
    }

    char *buf = (char *)calloc(1, sizeof(struct QP_info));
    if (!buf)
    {
        perror("callo for buf");
        goto out2;
    }

    // Send local IB information
    memcpy(buf, &(ctx->local_qp_info[0]), sizeof(struct QP_info)); // Only 1 remote server
    if (sokt_send(sockfd, buf, sizeof(struct QP_info)) == -1)
    {
        fprintf(stderr, "sokt_send failed\n");
        goto out3;
    }
    printf("local :\t\tlid: %d qpn: %-4d psn: %-9d addr: %ld rkey: %u\n",
           ctx->local_qp_info[0].lid, ctx->local_qp_info[0].qpn, ctx->local_qp_info[0].psn,
           ctx->local_qp_info[0].addr, ctx->local_qp_info[0].rkey);

    // Get remote IB information
    if (sokt_recv(sockfd, buf, sizeof(struct QP_info)) == -1)
    {
        fprintf(stderr, "sokt_recv failed\n");
        goto out3;
    }
    memcpy(&(ctx->remote_qp_info[0]), buf, sizeof(struct QP_info));
    printf("remote:\t\tlid: %d qpn: %-4d psn: %-9d addr: %ld rkey: %u\n",
           ctx->remote_qp_info[0].lid, ctx->remote_qp_info[0].qpn, ctx->remote_qp_info[0].psn,
           ctx->remote_qp_info[0].addr, ctx->remote_qp_info[0].rkey);

    // Setup RMDA connection
    if (connect_between_qps(ctx, 0))
    {
        fprintf(stderr, "connect_between_qps failed\n");
        goto out3;
    }

    rv = 0;

out3:
    free(buf);

out2:
    sokt_active_close(sockfd);

out1:
    return rv;
}

int connect_between_qps(struct rdma_context *ctx, int index)
{
    struct ibv_qp_attr qp_attr = {
        .qp_state = IBV_QPS_RTR,
        .path_mtu = IBV_MTU_4096,
        .dest_qp_num = ctx->remote_qp_info[index].qpn,
        .rq_psn = ctx->remote_qp_info[index].psn,
        .max_dest_rd_atomic = 1,
        .min_rnr_timer = 12,
        .ah_attr = {
            .is_global = 0,
            .dlid = ctx->remote_qp_info[index].lid,
            .sl = 0,
            .src_path_bits = 0,
            .port_num = IB_PORT}};
    int rtr_flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

    if (ibv_modify_qp(ctx->qp[index], &qp_attr, rtr_flags))
    {
        fprintf(stderr, "failed to modify QP %d to RTR\n", index);
        return -1;
    }

    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = IBV_QPS_RTS;
    qp_attr.sq_psn = ctx->local_qp_info[index].psn;
    qp_attr.timeout = 14;
    qp_attr.retry_cnt = 7;
    qp_attr.rnr_retry = 7;
    qp_attr.max_rd_atomic = 1;
    int rts_flags = IBV_QP_STATE | IBV_QP_SQ_PSN | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                    IBV_QP_MAX_QP_RD_ATOMIC;

    if (ibv_modify_qp(ctx->qp[index], &qp_attr, rts_flags))
    {
        fprintf(stderr, "failed to modify QP %d to RTS\n", index);
        return -1;
    }

    return 0;
}