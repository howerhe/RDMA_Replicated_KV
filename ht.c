#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "parameters.h"
#include "ht.h"

struct element
{
    char unused[CHUNK]; // To test how the size affect the RDMA throughput and latency
    ht_key_t key;
    ht_value_t value;
    struct element *next;
    int next_offset; // For backups, in element not byte for pointer arithmetic
};

struct ht
{
    unsigned bucket_num;
    unsigned element_num;
    unsigned element_num_internal; // element_num + hashtable list dummy heads + free list dummy head
    struct element *addr;          // start address for the elements
    struct element *free;          // dummy head for free list, which is placed immediately after bucket dummy heads
};

unsigned hash(const struct ht *ht, ht_key_t key);
// void *bucket_addr(const struct ht *ht, ht_key_t key);

struct ht *ht_create(int bucket_num, int element_num, void **addr, size_t *size)
{
    assert(0 < bucket_num && bucket_num <= (int)(HT_KEY_MAX - HT_KEY_MIN));
    assert(0 < element_num);

    struct ht *ht = calloc(1, sizeof(struct ht));
    if (!ht)
    {
        perror("calloc for ht");
        return NULL;
    }

    ht->bucket_num = bucket_num;
    ht->element_num = element_num;
    ht->element_num_internal = ht->element_num + ht->bucket_num + 1;

    ht->addr = calloc(ht->element_num_internal, sizeof(struct element));
    if (!ht->addr)
    {
        perror("calloc for ht->addr");
        free(ht);
        return NULL;
    }

    ht->free = ht->addr + ht->bucket_num;
    struct element *pre = ht->free;
    for (int i = ht->bucket_num + 1; i < ht->element_num_internal; i++)
    {
        pre->next = ht->addr + i;
        pre = pre->next;
    }

    if (addr)
    {
        *addr = ht->addr;
    }
    if (size)
    {
        *size = ht->element_num_internal * sizeof(struct element);
    }

    return ht;
}

void ht_destroy(struct ht *ht)
{
    if (!ht)
    {
        return;
    }

    if (ht->addr)
    {
        free(ht->addr);
    }

    free(ht);
}

void ht_show(struct ht *ht)
{
    assert(ht);

    struct element *e;

    for (int i = 0; i < ht->bucket_num; i++)
    {
        printf("bucket %d\n", i);
        e = ht->addr + i;

        int j = 0;
        while (e)
        {
            printf("\tnode %d\taddr %p\t", j, e);
            if (j == 0)
            {
                printf("dummy head\tnext %p\tnext_offset %d\n", e->next, e->next_offset);
            }
            else
            {
                printf("key %u\tvalue %u\tnext %p\tnext_offset %d\n", e->key, e->value, e->next, e->next_offset);
            }
            e = e->next;
            j++;
        }
        printf("\n");
    }

    e = ht->free;
    printf("free list\n");

    int j = 0;
    while (e)
    {
        printf("\tnode %d\taddr %p\t", j, e);
        if (j == 0)
        {
            printf("dummy head\n");
        }
        else
        {
            printf("empty element\n");
        }
        e = e->next;
        j++;
    }
}

// Not implemented
void ht_preload(struct ht *ht)
{
    assert(ht);
    /*
        for (int i = 0; i < ht->capacity; i++)
        {
            ht_key_t key = i + HT_KEY_MIN;
            struct element *addr = bucket_addr(ht, key);

            addr->occupied = 1;
            addr->key = key;
            addr->value = key;
        }*/
}

enum ht_code ht_put(const struct ht *ht, ht_key_t key, ht_value_t value, char *is_update, long *offsets, size_t *sizes)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);
    assert(HT_VALUE_MIN <= value && value <= HT_VALUE_MAX);

    struct element *pre = ht->addr + hash(ht, key);
    struct element *e = pre->next;

    while (e)
    {
        if (e->key == key) // Update
        {
            e->value = value;

            // update offset and size
            if (is_update)
            {
                *is_update = 1;
            }
            if (offsets)
            {
                *offsets = (e - ht->addr) * sizeof(struct element);
            }
            if (sizes)
            {
                *sizes = sizeof(struct element);
            }

            return HT_CODE_SUCCESS;
        }

        pre = e;
        e = e->next;
    }

    // Put
    e = ht->free->next;
    if (!e)
    {
        return HT_CODE_FULL;
    }

    ht->free->next = e->next;
    pre->next = e;
    pre->next_offset = e - ht->addr;
    e->next = NULL;
    e->key = key;
    e->value = value;

    // update offsets and sizes
    if (is_update)
    {
        *is_update = 0;
    }
    if (offsets)
    {
        offsets[0] = (e - ht->addr) * sizeof(struct element);
        offsets[1] = (pre - ht->addr) * sizeof(struct element);
    }
    if (sizes)
    {
        sizes[0] = sizeof(struct element);
        sizes[1] = sizeof(struct element);
    }

    return HT_CODE_SUCCESS;
}

// Not implemented
enum ht_code ht_del(const struct ht *ht, ht_key_t key, ht_value_t value, long *offset, size_t *size)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);
    assert(0);

    return HT_CODE_ERROR;
}

enum ht_code ht_get(const struct ht *ht, ht_key_t key, ht_value_t *value, char is_primary)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);
    assert(value);

    struct element *e = ht->addr + hash(ht, key);
    if (!is_primary && e->next)
    {
        e->next = ht->addr + e->next_offset;
    }
    e = e->next; // To skip the dummy head

    while (e)
    {
        if (e->key == key)
        {
            *value = e->value;
            return HT_CODE_SUCCESS;
        }

        if (!is_primary && e->next)
        {
            e->next = ht->addr + e->next_offset;
        }
        e = e->next;
    }

    return HT_CODE_NOT_FOUND;
}

unsigned hash(const struct ht *ht, ht_key_t key)
{
    assert(ht);
    assert(HT_KEY_MIN <= key && key <= HT_KEY_MAX);

    return key % ht->bucket_num;
}