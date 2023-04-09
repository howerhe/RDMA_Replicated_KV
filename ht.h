/*
 * Hashtable for RDMA
 */
#ifndef HT_H_
#define HT_H_

#include <stdint.h>

/**
 * @brief Hashtable
 *
 */
struct ht;

/**
 * @brief Hashtable element
 *
 */
struct ht_element;

/**
 * @brief Hashtable key
 *
 */
typedef uint8_t ht_key_t;

/**
 * @brief Hashtable key lower bound
 *
 */
#define HT_KEY_MIN 0

/**
 * @brief Hashtable key upper bound
 *
 */
#define HT_KEY_MAX UINT8_MAX

/**
 * @brief Hashtable value
 *
 */
typedef int32_t ht_value_t;

/**
 * @brief Hashtable value lower bound
 *
 */
#define HT_VALUE_MIN INT32_MIN

/**
 * @brief Hashtable value upper bound
 *
 */
#define HT_VALUE_MAX INT32_MAX

/**
 * @brief Create a hashtable
 *
 * @param bucket_num
 * @param bucket_size
 * @param ht_addr can be NULL
 * @param ht_size can be NULL
 * @return struct ht* NULL for failure
 */
struct ht *ht_create(int bucket_num, int bucket_size, struct ht_element **ht_addr, size_t *ht_size);

/**
 * @brief Destroy a hashtable
 *
 * @param ht
 */
void ht_destroy(struct ht *ht);

/**
 * @brief Print out the whole hashtable
 *
 * @param ht
 */
void ht_show(struct ht *ht);

/**
 * @brief Preload some data into the hashtable
 *
 * @param ht
 */
void ht_preload(struct ht *ht);

/**
 * @brief Return code for ht_put() and ht_get()
 *
 */
enum ht_code
{
    HT_CODE_SUCCESS,
    HT_CODE_ERROR,
    HT_CODE_FULL,
    HT_CODE_NOT_FOUND
};

/**
 * @brief Put a value for a given key
 *
 * @param ht
 * @param key
 * @param value
 * @param offset in byte, can be NULL
 * @param update_size can be NULL
 * @return enum ht_code
 */
enum ht_code ht_put(const struct ht *ht, ht_key_t key, ht_value_t value, long *offset, size_t *update_size);

/**
 * @brief Find the value for a given key
 *
 * @param ht
 * @param key
 * @param value
 * @param offset in byte, can be NULL
 * @param update_size can be NULL
 * @return enum ht_code
 */
enum ht_code ht_get(const struct ht *ht, ht_key_t key, ht_value_t *value, long *offset, size_t *update_size);

#endif