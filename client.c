#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "parameters.h"
#include "ht.h"
#include "sokt.h"

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

    // Initiate hash table
    struct ht *ht = ht_create(BUCKET_NUM, ELEMENT_NUM, NULL, NULL);
    if (!ht)
    {
        fprintf(stderr, "ht_create failed\n");
        goto out2;
    }

    ht_preload(ht);

    printf("hashtable initiated\n");

    // Setup socket connections with servers
    int *sockfds = malloc(servers_num * sizeof(int));
    if (!sockfds)
    {
        perror("malloc for sockfds");
        goto out3;
    }
    memset(sockfds, -1, servers_num * sizeof(int));

    for (int i = 0; i < servers_num; i++)
    {
        sockfds[i] = sokt_active_open(name_servers[i].addr, name_servers[i].port);
        if (sockfds[i] == -1)
        {
            fprintf(stderr, "sokt_active_open failed\n");
            goto out4;
        }
    }

    // Run the key-value store
    printf("\nrunning experiments\n");

    srand48(getpid() * time(NULL));

    struct sokt_message msg, buf;
    enum ht_code code;
    int server;

    for (int i = 0; i < TEST_NUM; i++)
    {
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

        // Send and recv
        memcpy(&buf, &msg, sizeof(struct sokt_message));
        if (sokt_send(sockfds[server], (char *)&buf, sizeof(struct sokt_message)) != 0)
        {
            fprintf(stderr, "sokt_send failed\n");
            continue;
        }

#ifdef LOG
        printf("to   server %d:\t", server);
        skot_message_show(&buf);
#endif

        if (sokt_recv(sockfds[server], (char *)&buf, sizeof(struct sokt_message)) != 0)
        {
            fprintf(stderr, "sokt_recv failed\n");
            continue;
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
            code = ht_get(ht, msg.key, &msg.value, 1);
            assert((buf.code == SOKT_CODE_SUCCESS && code == HT_CODE_SUCCESS) || (buf.code == SOKT_CODE_NOT_FOUND && code == HT_CODE_NOT_FOUND));
            assert(buf.key == msg.key);

            if (buf.code == SOKT_CODE_SUCCESS)
            {
                assert(buf.value == msg.value);
            }
        }
        else
        {
            fprintf(stderr, "wrong test\n");
        }
    }

    printf("\n");

    // Release resources
    rv = EXIT_SUCCESS;

out4:
    for (int i = 0; i < servers_num; i++)
    {
        if (sockfds[i] != -1)
        {
            sokt_active_close(sockfds[i]);
        }
    }

    free(sockfds);

out3:
    ht_destroy(ht);

out2:
    free(name_servers);

out1:
    return rv;
}