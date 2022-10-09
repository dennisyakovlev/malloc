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
void* my_malloc(size_t bytes);

// free memory previously requested with
// my_malloc
void  my_free(void* ptr);

// request num contiguos elements of
// size bytes
void* my_calloc(size_t num, size_t bytes);

void* my_realloc(void *ptr, size_t size);

void* my_reallocarray(void *ptr, size_t nmemb, size_t size);
