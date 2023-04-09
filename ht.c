#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ht.h"

#define PRELOAD_PERCENT 80

struct ht_element
{
    char is_occupied;
    ht_key_t key;
    ht_value_t value;
};

struct ht
{
    unsigned bucket_num;
    unsigned bucket_size;
    size_t capacity;
    struct ht_element *head;
};

unsigned hash(const struct ht *ht, ht_key_t key);
void *bucket_addr(const struct ht *ht, ht_key_t key);

struct ht *ht_create(int bucket_num, int bucket_size, struct ht_element **ht_addr, size_t *ht_size)
{
    assert(0 < bucket_num && bucket_num <= (int)(HT_KEY_MAX - HT_KEY_MIN));
    assert(0 < bucket_size);

    struct ht *ht = calloc(1, sizeof(struct ht));
    if (!ht)
    {
        perror("calloc for ht");
        return NULL;
    }

    ht->bucket_num = bucket_num;
    ht->bucket_size = bucket_size;
    ht->capacity = ht->bucket_num * ht->bucket_size;

    ht->head = calloc(ht->capacity, sizeof(struct ht_element));
    if (!ht->head)
    {
        perror("calloc for ht->head");
        free(ht);
        return NULL;
    }

    if (ht_addr)
    {
        *ht_addr = ht->head;
    }
    if (ht_size)
    {
        *ht_size = ht->capacity * sizeof(struct ht_element);
    }

    return ht;
}

void ht_destroy(struct ht *ht)
{
    if (!ht)
    {
        return;
    }

    if (ht->head)
    {
        free(ht->head);
    }

    free(ht);
}

void ht_show(struct ht *ht)
{
    assert(ht);

    for (int i = 0; i < ht->bucket_num; i++)
    {
        for (int j = 0; j < ht->bucket_size; j++)
        {
            struct ht_element *addr = ht->head + ht->bucket_size * i + j;
            printf("bucket %d column %d: address %p, is_occupied %d, key %u, value %u\n",
                   i, j, addr, addr->is_occupied, addr->key, addr->value);
        }
    }
}

void ht_preload(struct ht *ht)
{
    assert(ht);

    for (int i = 0; i < ht->capacity; i++)
    {
        ht_key_t key = i + HT_KEY_MIN;
        struct ht_element *addr = bucket_addr(ht, key);

        addr->is_occupied = 1;
        addr->key = key;
        addr->value = key;
    }
}

enum ht_code ht_put(const struct ht *ht, ht_key_t key, ht_value_t value, long *offset, size_t *update_size)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);
    assert(HT_VALUE_MIN <= value && value <= HT_VALUE_MAX);

    struct ht_element *addr = bucket_addr(ht, key);
    if (!addr)
    {
        fprintf(stderr, "bucket_addr failed\n");
        return HT_CODE_ERROR;
    }

    for (int i = 0; i < ht->bucket_size; i++)
    {
        if (addr[i].is_occupied)
        {
            continue;
        }

        addr[i].is_occupied = 1;
        addr[i].key = key;
        addr[i].value = value;

        if (offset)
        {
            *offset = (&(addr[i]) - ht->head) * sizeof(struct ht_element);
        }
        if (update_size)
        {
            *update_size = sizeof(struct ht_element);
        }
        return HT_CODE_SUCCESS;
    }

    return HT_CODE_FULL;
}

enum ht_code ht_get(const struct ht *ht, ht_key_t key, ht_value_t *value, long *offset, size_t *update_size)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);
    assert(value);

    struct ht_element *addr = bucket_addr(ht, key);
    if (!addr)
    {
        fprintf(stderr, "bucket_addr failed\n");
        return HT_CODE_ERROR;
    }

    for (int i = 0; i < ht->bucket_size; i++)
    {
        if (!addr[i].is_occupied)
        {
            break;
        }
        if (addr[i].key != key)
        {
            continue;
        }

        *value = addr[i].value;
        if (offset)
        {
            *offset = (&(addr[i]) - ht->head) * sizeof(struct ht_element);
        }
        if (update_size)
        {
            *update_size = sizeof(struct ht_element);
        }
        return HT_CODE_SUCCESS;
    }

    return HT_CODE_NOT_FOUND;
}

unsigned hash(const struct ht *ht, ht_key_t key)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);

    return key % ht->bucket_num;
}

void *bucket_addr(const struct ht *ht, ht_key_t key)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);

    unsigned bucket = hash(ht, key);

    return ht->head + bucket * ht->bucket_size;
}