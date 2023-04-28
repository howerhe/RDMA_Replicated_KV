#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "parameters.h"
#include "ht.h"
#include "sokt.h"

// Experiment record
struct log
{
    int requests;
    double latency;
};

// Length of the log
#define LOG_LENGTH (TEST_NUM / STATISTICS_CYCLE - 1)

struct client_routine_info
{
    struct sokt_name_info *name_client;
    struct sokt_name_info *name_servers;
    int servers_num;
    int index;
};

void *client_routine(void *info);

int main(int argc, char *argv[])
{
    int rv = EXIT_FAILURE;

    // Parse arguments
    if (argc <= 5 || argc % 2 == 0)
    {
        fprintf(stderr, "Usage: client self_addr self_port "
                        "parimary_serv_addr primary_serv_port "
                        "backup_serv_addr_1 backup_serv_port1 ...\n");
        goto out1;
    }

    struct sokt_name_info name_client, *name_servers;

    name_client.addr = argv[1];
    name_client.port = argv[2];

    int servers_num = (argc - 3) / 2;
    name_servers = calloc(servers_num, sizeof(struct sokt_name_info));
    if (!name_servers)
    {
        perror("calloc for name_servers");
        goto out1;
    }

    for (int i = 0; i < servers_num; i++)
    {
        name_servers[i].addr = argv[3 + 2 * i];
        name_servers[i].port = argv[3 + 2 * i + 1];
    }

    printf("client\t\t%s:%s\n", name_client.addr, name_client.port);
    printf("primary\t\t%s:%s\n", name_servers[0].addr, name_servers[0].port);
    for (int i = 1; i < servers_num; i++)
    {
        printf("backup%d\t\t%s:%s\n", i - 1, name_servers[i].addr, name_servers[i].port);
    }
    printf("\n");

    pthread_t tids[CLIENT_THREAD];
    struct client_routine_info info[CLIENT_THREAD];

    // Statistics
    struct timeval start, end;
    gettimeofday(&start, NULL);

    for (int i = 0; i < CLIENT_THREAD; i++)
    {
        info[i].name_client = &name_client;
        info[i].name_servers = name_servers;
        info[i].servers_num = servers_num;
        info[i].index = i;

        if (pthread_create(&tids[i], NULL, client_routine, &info[i]) != 0)
        {
            perror("pthread_create");
            goto out2;
        }
    }

    for (int i = 0; i < CLIENT_THREAD; i++)
    {
        if (pthread_join(tids[i], NULL) != 0)
        {
            perror("pthread_join");
            goto out2;
        }
    }

    gettimeofday(&end, NULL);
    double us = (end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);
    printf("throughput is %.3f ops/ms\n", TEST_NUM * CLIENT_THREAD / (us / 1000));

    // Release resources
    rv = EXIT_SUCCESS;

out2:
    free(name_servers);

out1:
    return rv;
}

void *client_routine(void *info)
{
    struct sokt_name_info *name_client = ((struct client_routine_info *)info)->name_client;
    struct sokt_name_info *name_servers = ((struct client_routine_info *)info)->name_servers;
    int servers_num = ((struct client_routine_info *)info)->servers_num;
    int index = ((struct client_routine_info *)info)->index;

    // Initiate hash table
    struct ht *ht = ht_create(BUCKET_NUM, ELEMENT_NUM, NULL, NULL);
    if (!ht)
    {
        fprintf(stderr, "ht_create failed\n");
        goto out1;
    }

    ht_preload(ht);

    printf("hashtable initiated for thread %d\n", index);

    // Run the key-value store
    printf("running experiments for thread %d\n", index);

    srand48(getpid() * time(NULL));

    struct sokt_message msg, buf;
    enum ht_code code;
    int server;

    // Statistics
    struct log log[LOG_LENGTH];
    int n_req = 0;
    struct timeval start, end;

    gettimeofday(&start, NULL);
    for (int i = 0; i < TEST_NUM; i++)
    {
        // Statistics
        if (n_req % STATISTICS_CYCLE == 0 && n_req != 0)
        {
            gettimeofday(&end, NULL);
            double us = (end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec);

            int index = n_req / STATISTICS_CYCLE - 1;
            log[index].requests = n_req;
            log[index].latency = us / 1000;

            gettimeofday(&start, NULL);
        }

        /*
        // Test that shows backup can work
        switch (i % 3)
        {
        case 0:
            msg.key = i / 3;
            msg.value = msg.key;
            msg.code = SOKT_CODE_PUT;
            server = 0;
            break;
        case 1:
            msg.key = i / 3;
            msg.value = -1;
            msg.code = SOKT_CODE_GET;
            server = 0;
            break;
        case 2:
            msg.key = i / 3;
            msg.value = -1;
            msg.code = SOKT_CODE_GET;
            server = 1;
            break;
        }
        */

        // Generate the code, key, value and server index for test
        msg.key = rand() % (HT_KEY_MAX - HT_KEY_MIN) + HT_KEY_MIN;
        msg.code = rand() % 100;
        if (msg.code < PUT_PERCENT)
        {
            msg.code = SOKT_CODE_PUT;
            msg.value = rand() % HT_VALUE_MAX;
            server = 0;
        }
        else
        {
            msg.code = SOKT_CODE_GET;
            msg.value = -1;
            server = rand() % servers_num;
        }

        int sockfd = sokt_active_open(name_servers[server].addr, name_servers[server].port);
        if (sockfd == -1)
        {
            fprintf(stderr, "sokt_active_open failed\n");
            goto loop_clean;
        }

        // Send and recv
        memcpy(&buf, &msg, sizeof(struct sokt_message));
        if (sokt_send(sockfd, (char *)&buf, sizeof(struct sokt_message)) != 0)
        {
            fprintf(stderr, "sokt_send failed\n");
            goto loop_clean;
        }

#ifdef LOG
        printf("to   server %d:\t", server);
        skot_message_show(&buf);
#endif

        if (sokt_recv(sockfd, (char *)&buf, sizeof(struct sokt_message)) != 0)
        {
            fprintf(stderr, "sokt_recv failed\n");
            goto loop_clean;
        }

#ifdef LOG
        printf("from server %d:\t", server);
        skot_message_show(&buf);
#endif

        // Validate the returned information
        if (msg.code == SOKT_CODE_PUT)
        {
            code = ht_put(ht, msg.key, msg.value, NULL, NULL, NULL);
            assert((buf.code == SOKT_CODE_SUCCESS && code == HT_CODE_SUCCESS) || (buf.code == SOKT_CODE_FULL && code == HT_CODE_FULL));
            assert(buf.key == msg.key);
            assert(buf.value == msg.value);

#ifdef LOG
            if (buf.code == SOKT_CODE_FULL)
            {
                printf("hashtable is full for key %d\n", buf.key);
            }
#endif
        }
        else if (msg.code == SOKT_CODE_GET)
        {
            // These are not always true when there are multiple clients
            // code = ht_get(ht, msg.key, &msg.value, 1);
            // assert((buf.code == SOKT_CODE_SUCCESS && code == HT_CODE_SUCCESS) || (buf.code == SOKT_CODE_NOT_FOUND && code == HT_CODE_NOT_FOUND));
            // assert(buf.key == msg.key);

            // if (buf.code == SOKT_CODE_SUCCESS)
            // {
            //     assert(buf.value == msg.value);
            // }
        }
        else
        {
            fprintf(stderr, "wrong test\n");
        }

    loop_clean:
        sokt_active_close(sockfd);

        n_req++;
    }

    printf("\n");

    // Write the statistics into file
    char file_name[20];
    sprintf(file_name, "%s_%s_%d.csv", "client", name_client->port, index);
    FILE *fp = fopen(file_name, "w");
    assert(fp);

    fprintf(fp, "requests, latency (ms)\n");
    for (int i = 0; i < LOG_LENGTH; i++)
    {
        fprintf(fp, "%d, %.3f\n", log[i].requests, log[i].latency);
    }

    fclose(fp);

    ht_destroy(ht);

out1:
    return NULL;
}