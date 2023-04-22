#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "parameters.h"
#include "pool.h"
#include "ht.h"
#include "rdma.h"
#include "sokt.h"

struct handle_client_info
{
    int unused; // For padding pupose; without this connfd cannot be passed to the thread pool correctly
    int connfd;
    char is_primary;
    pthread_rwlock_t *rwlock;
    struct ht *ht;
    struct rdma_context *rdma_ctx;
    int others_num;
};

void *handle_client(void *info);

int main(int argc, char *argv[])
{
    int rv = EXIT_FAILURE;

    // Parse arguments
    if (argc <= 4 || argc % 2 == 1)
    {
        fprintf(stderr, "Usage: server is_primary self_addr self_port "
                        "others_addr_1 others_port_1 ...\n");
        goto out1;
    }

    char is_primary = atoi(argv[1]) == 1 ? 1 : 0;
    struct sokt_name_info name_self, *name_others;

    name_self.addr = argv[2];
    name_self.port = argv[3];

    int others_num = (argc - 4) / 2;
    name_others = calloc(others_num, sizeof(struct sokt_name_info));
    if (!name_others)
    {
        perror("calloc for name_others");
        goto out1;
    }

    for (int i = 0; i < others_num; i++)
    {
        name_others[i].addr = argv[4 + 2 * i];
        name_others[i].port = argv[4 + 2 * i + 1];
    }

    if (is_primary)
    {
        printf("primary\t\t%s:%s\n", name_self.addr, name_self.port);
        for (int i = 0; i < others_num; i++)
        {
            printf("backup%d\t\t%s:%s\n", i, name_others[i].addr, name_others[i].port);
        }
    }
    else
    {
        printf("backup\t\t%s:%s\n", name_self.addr, name_self.port);
        printf("primary\t\t%s:%s\n", name_others[0].addr, name_others[0].port);
    }
    printf("\n");

    // For multithreading
    pool_t pool = pool_init(5);
    if (pool == NULL)
    {
        printf("pool_init failed\n");
        goto out2;
        return -1;
    }

    pthread_rwlock_t rwlock[HT_KEY_MAX - HT_KEY_MIN + 1];
    for (long i = 0; i < HT_KEY_MAX - HT_KEY_MIN + 1; i++)
    {
        if (pthread_rwlock_init(&rwlock[i], NULL) != 0)
        {
            perror("pthread_rwlock_init");
            goto out3;
        }
    }

    // Initiate hash table
    void *ht_addr;
    size_t ht_size;

    struct ht *ht = ht_create(BUCKET_NUM, ELEMENT_NUM, &ht_addr, &ht_size);
    if (!ht)
    {
        fprintf(stderr, "ht_create failed\n");
        goto out4;
    }

    ht_preload(ht);

    printf("hashtable initiated at %p with size %lu\n", (void *)ht_addr, ht_size);

    // Setup socket connections for clients and backup RDMA connection
    int sockfd = sokt_passive_open(NULL, name_self.port);
    if (sockfd == -1)
    {
        fprintf(stderr, "sokt_passive_open failed\n");
        goto out5;
    }

    // Setup RDMA connections with other servers
    struct rdma_context *rdma_ctx = rdma_open_connection(is_primary, sockfd, &name_others, others_num, ht_addr, ht_size);
    if (!rdma_ctx)
    {
        fprintf(stderr, "rdma_open_connection failed\n");
        goto out6;
    }

    // Run the key-value store
    printf("\nrunning experiments\n");

    while (1)
    {
        struct handle_client_info *info = calloc(1, sizeof(struct handle_client_info));
        if (!info)
        {
            perror("calloc for info");
            continue;
        }

        info->connfd = sokt_passive_accept_open(sockfd);
        if (info->connfd == -1)
        {
            fprintf(stderr, "sokt_passive_accept_open failed\n");
            free(info);
            continue;
        }

        info->is_primary = is_primary;
        info->rwlock = rwlock;
        info->ht = ht;
        info->rdma_ctx = rdma_ctx;
        info->others_num = others_num;

        if (pool_add(pool, handle_client, info) == -1)
        {
            fprintf(stderr, "pool_add failed\n");
        }
    }

    // Release resources
    rv = EXIT_SUCCESS;

    rdma_close_connection(rdma_ctx, others_num);

out6:
    sokt_passive_close(sockfd);

out5:
    ht_destroy(ht);

out4:
    for (long i = 0; i < HT_KEY_MAX - HT_KEY_MIN + 1; i++)
    {
        pthread_rwlock_destroy(&rwlock[i]);
    }

out3:
    pool_free(pool);

out2:
    if (name_others)
    {
        free(name_others);
    }

out1:
    return rv;
}

void *handle_client(void *info)
{
    int connfd = ((struct handle_client_info *)info)->connfd;
    char is_primary = ((struct handle_client_info *)info)->is_primary;
    pthread_rwlock_t *rwlock = ((struct handle_client_info *)info)->rwlock;
    struct ht *ht = ((struct handle_client_info *)info)->ht;
    struct rdma_context *rdma_ctx = ((struct handle_client_info *)info)->rdma_ctx;
    int others_num = ((struct handle_client_info *)info)->others_num;

    free(info);

    struct sokt_message msg;
    int ht_status;
    char is_update;
    long ht_element_offset[2];
    size_t ht_element_size[2];
    enum sokt_message_code code;

    if (sokt_recv(connfd, (char *)&msg, sizeof(struct sokt_message)) != 0)
    {
        fprintf(stderr, "sokt_recv failed\n");
        goto handle_client_out_1;
    }

#ifdef LOG
    printf("from client:\t");
    skot_message_show(&msg);
#endif

    code = msg.code;
    if (code == SOKT_CODE_PUT && is_primary)
    {
        if (pthread_rwlock_wrlock(&rwlock[msg.key]) == 0)
        {

            ht_status = ht_put(ht, msg.key, msg.value, &is_update, ht_element_offset, ht_element_size);
            switch (ht_status)
            {
            case HT_CODE_SUCCESS:
                msg.code = SOKT_CODE_SUCCESS;
                break;
            case HT_CODE_FULL:
                msg.code = SOKT_CODE_FULL;
                break;
            default:
                msg.code = SOKT_CODE_ERROR;
                break;
            }
        }
        else
        {
            perror("pthread_rwlock_wrlock");
            msg.code = SOKT_CODE_ERROR;
        }
    }
    else if (code == SOKT_CODE_GET)
    {
        if (pthread_rwlock_rdlock(&rwlock[msg.key]) == 0)
        {

            ht_status = ht_get(ht, msg.key, &msg.value, is_primary);
            switch (ht_status)
            {
            case HT_CODE_SUCCESS:
                msg.code = SOKT_CODE_SUCCESS;
                break;
            case HT_CODE_NOT_FOUND:
                msg.code = SOKT_CODE_NOT_FOUND;
                break;
            default:
                msg.code = SOKT_CODE_ERROR;
                break;
            }
        }
        else
        {
            perror("pthread_rwlock_rdlock");
            msg.code = SOKT_CODE_ERROR;
        }
    }
    else
    {
        msg.code = SOKT_CODE_ERROR;
    }

    if (sokt_send(connfd, (char *)&msg, sizeof(struct sokt_message)) != 0)
    {
        fprintf(stderr, "sokt_send failed\n");
        goto handle_client_out_2;
    }

#ifdef LOG
    printf("to   client:\t");
    skot_message_show(&msg);
#endif

    if (code == SOKT_CODE_PUT && msg.code == SOKT_CODE_SUCCESS && is_primary)
    {
        if (rdma_wrtie_all(rdma_ctx, ht_element_offset[0], ht_element_size[0], others_num) == -1)
        {
            fprintf(stderr, "rdma_wrtie_all failed\n");
            goto handle_client_out_2;
        }

        if (rdma_wait_completion_all(rdma_ctx, others_num) == -1)
        {
            fprintf(stderr, "rdma_wait_completion_all failed\n");
            goto handle_client_out_2;
        }

        if (is_update == 0)
        {
            if (rdma_wrtie_all(rdma_ctx, ht_element_offset[1], ht_element_size[1], others_num) == -1)
            {
                fprintf(stderr, "rdma_wrtie_all failed\n");
                goto handle_client_out_2;
            }

            if (rdma_wait_completion_all(rdma_ctx, others_num) == -1)
            {
                fprintf(stderr, "rdma_wait_completion_all failed\n");
                goto handle_client_out_2;
            }
        }
    }

handle_client_out_2:
    if (pthread_rwlock_unlock(&rwlock[msg.key]) != 0)
    {
        perror("pthread_rwlock_unlock"); // Should rarely happen
    }

handle_client_out_1:
    sokt_passive_accept_close(connfd);

    return NULL;
}