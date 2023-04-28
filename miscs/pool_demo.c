#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pool.h"

#define CYCLE 300

struct demo_info
{
    unsigned id;
    long parameter;
};

void *fun(void *args)
{
    unsigned id = ((struct demo_info *)args)->id;
    long parameter = ((struct demo_info *)args)->parameter;

    printf("running the thread of %u with parameter %ld\n", id, parameter);

    return NULL;
}

int main(void)
{
    pool_t pool = pool_init(5);
    if (pool == NULL)
    {
        printf("pool_init failed!\n");
        return -1;
    }

    struct demo_info info[CYCLE];
    memset(info, 0, sizeof(struct demo_info) * CYCLE);
    for (long i = 0; i < CYCLE; i++)
    {
        info[i].parameter = i;
        pool_add(pool, fun, &info[i]);
    }

    sleep(2);
    pool_free(pool);
    pool = NULL;

    return 0;
}