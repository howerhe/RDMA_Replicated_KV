// Reference (in Chinese): https://zhuanlan.zhihu.com/p/44971598

#ifndef POOL_H
#define POOL_H

/**
 * @brief Thread pool to handle different tasks.
 *
 */
typedef struct pool *pool_t;

/**
 * @brief Initiate a thread pool with the given size. Each thread will have a
 * unique ID (unsigned).
 *
 * @param size size of the thread pool
 * @return thread pool, NULL when errors occur
 */
pool_t pool_init(unsigned size);

/**
 * @brief Free and clean a thread pool.
 *
 * @param pool thread pool
 */
void pool_free(pool_t pool);

/**
 * @brief Add a new task to the thread pool. A task contains a routine and an
 * argument. The task will be executed if there is a free thread.
 *
 * @param pool thread pool
 * @param routine function pointer to the routine
 * @param args pointer to the arguments for the routine, the first member of the
 * args must be an unsigned representing the ID of the worker thread
 * @return 0 when no error occurs
 */
int pool_add(pool_t pool, void *(*routine)(void *), void *args);

#endif