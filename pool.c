#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pool.h"

struct pool_task
{
    void *(*routine)(void *);
    void *args;
    struct pool_task *next;
};

struct pool
{
    int is_over;
    unsigned size;
    pthread_t *ids;
    struct pool_task *head;
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

struct task_info
{
    struct pool *pool;
    unsigned id;
};

// Why static?
static void *handler(void *args)
{
    struct pool *pool = ((struct task_info *)args)->pool;
    unsigned id = ((struct task_info *)args)->id;
    struct pool_task *task = NULL;
    free(args);
    args = NULL;

    while (1)
    {
        pthread_mutex_lock(&pool->lock);

        while (pool->head == NULL && pool->is_over != 1)
            pthread_cond_wait(&pool->cond, &pool->lock);

        if (pool->is_over == 1)
        {
            pthread_mutex_unlock(&pool->lock);
            pthread_exit(NULL);
        }

        task = pool->head;
        pool->head = pool->head->next;

        pthread_mutex_unlock(&pool->lock);

        // The first 1 byte of args is always the thread id (unsigned).

        // TODO: find a better way since this highly depends on compiler to
        // arrage each field in a struc.
        memcpy(task->args, &id, sizeof(unsigned));
        task->routine(task->args);
        free(task);
    }

    return NULL;
}

pool_t pool_init(unsigned size)
{
    struct pool *pool = malloc(sizeof(struct pool));
    if (pool == NULL)
        goto error;

    pool->is_over = 0;
    pool->size = size;
    pool->ids = malloc(sizeof(pthread_t) * pool->size);
    if (pool->ids == NULL)
        goto error;
    pool->head = NULL;
    if (pthread_mutex_init(&(pool->lock), NULL) != 0)
        goto error;
    if (pthread_cond_init(&(pool->cond), NULL) != 0)
        goto error;

    for (int i = 0; i < pool->size; i++)
    {
        struct task_info *info = malloc(sizeof(struct task_info));
        info->pool = pool;
        info->id = i;
        if (pthread_create(&(pool->ids[i]), NULL, handler, info) != 0)
            goto error;
    }

    return pool;

error:
    perror("pool_init");
    pool_free(pool);
    pool = NULL;
    return NULL;
}

void pool_free(pool_t pool)
{
    if (pool == NULL)
        return;
    if (pool->is_over == 1)
        return;

    pool->is_over = 1;

    pthread_mutex_lock(&pool->lock);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < pool->size; i++)
        pthread_join(pool->ids[i], NULL);
    free(pool->ids);
    pool->ids = NULL;

    struct pool_task *tmp;
    while (pool->head)
    {
        tmp = pool->head;
        pool->head = pool->head->next;
        free(tmp);
        tmp = NULL;
    }

    pthread_cond_destroy(&pool->cond);
    pthread_mutex_destroy(&pool->lock);
    free(pool);
    pool = NULL;
}

int pool_add(pool_t pool, void *(*routine)(void *), void *args)
{
    char *err_msg = NULL;

    if (pool == NULL)
    {
        err_msg = "pool is NULL";
        goto error;
    }
    if (routine == NULL)
    {
        err_msg = "routine is NULL";
        goto error;
    }

    struct pool_task *cur, *tmp;
    cur = malloc(sizeof(struct pool_task));
    if (cur == NULL)
        goto error;

    cur->routine = routine;
    cur->args = args;
    cur->next = NULL;

    pthread_mutex_lock(&pool->lock);

    tmp = pool->head;
    if (tmp == NULL)
    {
        pool->head = cur;
    }
    else
    {
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = cur;
    }
    tmp = NULL;

    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->lock);

    return 0;

error:
    if (err_msg != NULL)
        fprintf(stderr, "pool_add: %s\n", err_msg);
    else
        perror("pool_add");
    return -1;
}