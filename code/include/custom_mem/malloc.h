#pragma once
#include <stddef.h>
#include <time.h>

struct MallocAdjustables
{
    /* Minimum amount of more memory to request at a time.
    */
    size_t more_mem;

    /*  Global waiting time. 
    */
    struct timespec long_wait;
};

// request n bytes of contiguous memory
void* my_malloc(size_t);

// free memory previously requested with
// my_malloc
void my_free(void*);
