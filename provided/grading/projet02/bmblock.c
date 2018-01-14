/**
* @file bmblock.h
* @brief utility functions for the UNIX v6 filesystem.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bmblock.h"
#include "error.h"

#define ELE_PER_INDEX (64)
#define CLEAR (0)
#define GET_SHADOW (1)
#define BYTE_SIZE (8)

/**
* @brief allocate a new bmblock_array to handle elements indexed
* between min and max (included, thus (max-min+1) elements).
* @param min the mininum value supported by our bmblock_array
* @param max the maxinum value supported by our bmblock_array
* @return a pointer of the newly created bmblock_array or NULL on failure
*/
struct bmblock_array* bm_alloc(uint64_t min, uint64_t max)
{
    struct bmblock_array* bm = NULL;

    if (min <= max) {
        // Only max - min because already 1 elem in struct bmblock_array.
        bm = calloc(1, sizeof(struct bmblock_array) + (max - min) * sizeof(uint64_t));

        if (bm != NULL) {
            bm->length = (max - min) / ELE_PER_INDEX + 1;
            bm->cursor = 0;
            bm->min = min;
            bm->max = max;
        }
    }

    return bm;
}

/**
* @brief return the bit associated to the given value
* @param bmblock_array the array containing the value we want to read
* @param x an integer corresponding to the number of the value we are looking for
* @return <0 on failure, 0 or 1 on success
*/
int bm_get(struct bmblock_array* bmblock_array, uint64_t x)
{
    M_REQUIRE_NON_NULL(bmblock_array);

    if (x < bmblock_array->min || x > bmblock_array->max) {
        return ERR_BAD_PARAMETER;
    }

    size_t i = (x - bmblock_array->min) / ELE_PER_INDEX;
    size_t off = (x - bmblock_array->min) % ELE_PER_INDEX;

    return (bmblock_array->bm[i] >> (ELE_PER_INDEX - off)) & GET_SHADOW;
}

/**
* @brief set to true (or 1) the bit associated to the given value
* @param bmblock_array the array containing the value we want to set
* @param x an integer corresponding to the number of the value we are looking for
*/
void bm_set(struct bmblock_array* bmblock_array, uint64_t x)
{
    if (bmblock_array != NULL && bmblock_array->min <= x && x <= bmblock_array->max) {
        size_t i = (x - bmblock_array->min) / ELE_PER_INDEX;
        size_t off = (x - bmblock_array->min) % ELE_PER_INDEX;

        bmblock_array->bm[i] |= (UINT64_C(1) << (ELE_PER_INDEX - off));
    }
}

/**
* @brief set to false (or 0) the bit associated to the given value
* @param bmblock_array the array containing the value we want to clear
* @param x an integer corresponding to the number of the value we are looking for
*/
void bm_clear(struct bmblock_array* bmblock_array, uint64_t x)
{
    if (bmblock_array != NULL && bmblock_array->min <= x && x <= bmblock_array->max) {

        size_t i = (x - bmblock_array->min) / ELE_PER_INDEX;
        size_t off = (x - bmblock_array->min) % ELE_PER_INDEX;

        bmblock_array->bm[i] &= (~(UINT64_C(1) << (ELE_PER_INDEX - off)));

        if (bmblock_array->cursor > (x - bmblock_array->min) / ELE_PER_INDEX) {
            bmblock_array->cursor = (x - bmblock_array->min) / ELE_PER_INDEX;
        }
    }
}

/**
* @brief prints the bits of one uint64_t element
* @param bm the pointer to the bmblock_array struct
* @param ind the uint64_t element
*/
void bm_print_bit(struct bmblock_array* bm, uint64_t ind)
{
    if (bm != NULL) {
        int counter = 0;

        for (uint64_t i = (ind * ELE_PER_INDEX) + bm->min; i < ((ind + 1) * ELE_PER_INDEX) + bm->min; ++i) {
            if (i > bm->max) {
                printf("%d", CLEAR);
            } else {
                printf("%d", bm_get(bm, i));
            }

            counter += 1;

            if (counter % BYTE_SIZE == 0) {
                printf(" ");
            }
        }
    }
}

/**
 * @brief usefull to see (and debug) content of a bmblock_array
 * @param bmblock_array the array we want to see
 */
void bm_print(struct bmblock_array *bmblock_array)
{
    printf("**********BitMap Block START**********\n");

    if (bmblock_array != NULL) {
        printf("lenght: %zu\n", bmblock_array->length);
        printf("min: %zu\n", bmblock_array->min);
        printf("max: %zu\n", bmblock_array->max);
        printf("cursor: %zu\n", bmblock_array->cursor);
        printf("content:\n");

        for (uint64_t i = 0; i < bmblock_array->length; ++i) {
            printf("%zu: ", i);
            bm_print_bit(bmblock_array, i);
            printf("\n");
        }
    } else {
        printf("NULL ptr");
    }

    printf("**********BitMap Block END************\n");

    fflush(stdout);
}

/**
* @brief return the next unused bit
* @param bmblock_array the array we want to search for place
* @return <0 on failure, the value of the next unused value otherwise
*/
int bm_find_next(struct bmblock_array *bmblock_array)
{
    M_REQUIRE_NON_NULL(bmblock_array);

    uint64_t i = bmblock_array->cursor * BYTE_SIZE;
    while (bm_get(bmblock_array, i) && i <= bmblock_array->max) {
        i += 1;
    }

    if (i <= bmblock_array->max) {
        bmblock_array->cursor = (i - bmblock_array->min) / ELE_PER_INDEX;

        return (int) i;
    }

    return ERR_BITMAP_FULL;
}
