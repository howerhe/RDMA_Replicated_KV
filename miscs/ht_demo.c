#include <stdio.h>

#include "ht.h"

int main()
{
    void *addr;
    size_t size;
    struct ht *ht = ht_create(10, 20, &addr, &size);

    printf("hashtable start addr %p, size %lu\n\n", addr, size);
    ht_show(ht);
    printf("\n");

    for (int i = 0; i < 2; i++)
    {
        printf("put (5, 10)\n");
        char is_update;
        long offsets[2] = {0, 0};
        size_t sizes[2] = {0, 0};
        ht_put(ht, 5, 10, &is_update, offsets, sizes);
        printf("is_update %d, offsets[0] %ld, offsets[1] %ld, sizes[0] %lu, sized[1] %lu\n",
               is_update, offsets[0], offsets[1], sizes[0], sizes[1]);

        printf("get 5\n");
        ht_value_t value;
        ht_get(ht, 5, &value, 1);
        printf("value %u\n", value);
        ht_show(ht);
        printf("\n");
    }

    printf("put (15, 20)\n");
    char is_update;
    long offsets[2];
    size_t sizes[2];
    ht_put(ht, 15, 20, &is_update, offsets, sizes);
    printf("is_update %d, offsets[0] %ld, offsets[1] %ld, sizes[0] %lu, sized[1] %lu\n",
           is_update, offsets[0], offsets[1], sizes[0], sizes[1]);

    printf("get 15 from primary\n");
    ht_value_t value;
    ht_get(ht, 15, &value, 1);
    printf("value %u\n", value);
    ht_show(ht);
    printf("\n");

    printf("get 15 from backup\n");
    ht_get(ht, 15, &value, 0);
    printf("value %u\n", value);
    ht_show(ht);

    ht_destroy(ht);
    return 0;
}