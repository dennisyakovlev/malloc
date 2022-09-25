#pragma once
#include <stddef.h>

struct MallocAdjustables
{
    /* Minimum amount of more memory to request at a time.
    */
    size_t more_mem;

    /* Fast cache size.
    */
    size_t cache_sz;
};

// request n bytes of contiguous memory
void* my_malloc(size_t);

// free memory previously requested with
// my_malloc
void my_free(void*);
