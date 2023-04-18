/*
 * Hashtable for RDMA
 */
#ifndef HT_H_
#define HT_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Hashtable
 *
 */
struct ht;

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
 * @param bucket_num the number of buckets
 * @param element_num the number of elements the hashtable can hold
 * @param addr the starting address of the hashtable in memory, can be NULL
 * @param size the memory space taken by the hashtable, can be NULL
 * @return struct ht*
 */
struct ht *ht_create(int bucket_num, int element_num, void **addr, size_t *size);

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
 * @brief Return code for ht_put(), ht_get() and ht_del()
 *
 */
enum ht_code
{
    HT_CODE_SUCCESS,
    HT_CODE_ERROR,
    HT_CODE_NOT_FOUND,
    HT_CODE_FULL
};

/**
 * @brief Put a value for a given key (note that backups do not need to update free list pointers)
 *
 * @param ht
 * @param key
 * @param value
 * @param is_update if the operation is a update or a real put, can be NULL
 * @param offsets offsets of the starting address in memory, in byte, should be an array of size 2, can be NULL
 * @param sizes memory affected, should be an array of size 2, can be NULL
 * @return enum ht_code
 */
enum ht_code ht_put(const struct ht *ht, ht_key_t key, ht_value_t value, char *is_update, long *offsets, size_t *sizes);

/**
 * @brief Delete a value for a given key (note that backups do not need to update free list pointers; the size of offset and size are both 1)
 *
 * @param ht
 * @param key
 * @param offset offset of the starting address in memory, in byte, can be NULL
 * @param size memory affected, can be NULL
 * @return enum ht_code
 */
enum ht_code ht_del(const struct ht *ht, ht_key_t key, ht_value_t value, long *offset, size_t *size);

/**
 * @brief Find the value for a given key
 *
 * @param ht
 * @param key
 * @param value
 * @param is_backup
 * @return enum ht_code
 */
enum ht_code ht_get(const struct ht *ht, ht_key_t key, ht_value_t *value, char is_primary);

#endif