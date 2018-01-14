#include <stdio.h>
#include <stdlib.h>
#include "bmblock.h"

#define MIN (4)
#define MAX (131)
#define INCR_3 (3)
#define INCR_5 (5)

int main(void)
{
    struct bmblock_array* bm = bm_alloc(MIN, MAX);

    bm_print(bm);
    printf("find_next() = %d\n", bm_find_next(bm));

    bm_set(bm, MIN);
    bm_set(bm, MIN + 1);
    bm_set(bm, MIN + 2);

    bm_print(bm);
    printf("find_next() = %d\n", bm_find_next(bm));

    for (uint64_t i = MIN; i < MAX; i += INCR_3) {
        bm_set(bm, i);
    }

    bm_print(bm);
    printf("find_next() = %d\n", bm_find_next(bm));

    for (uint64_t i = MIN + 1; i < MAX; i+= INCR_5) {
        bm_clear(bm, i);
    }

    bm_print(bm);
    printf("find_next() = %d\n", bm_find_next(bm));

    free(bm);

    return 0;
}
