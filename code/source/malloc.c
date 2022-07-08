/*  Simple malloc implementation.

    Allocation works by assigning an int at start of every
    requested block. The number stores size of requested memory
    and whether it is taken. Negative means free, positive
    means taken.

    Pitfalls
        - if requested number of bytes is greater than MALLOC_SIZE, will fail
          ungracefully
        - when runs out of memory, fails ungracefully
        - doesn't work with mmap since operates on program break
        - there's no free, realloc, calloc functions to go with malloc
        - extra memory usage if requested size if < MALLOC_SIZE
        - extra extra memory usage is sbrk returns > MALLOC_SIZE 
    Strengths
        - by virtue of lack of features, no fragmentation
        - only has overhead of sizeof(void*)

*/
#include <custom_mem/malloc.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>

#define MALLOC_SIZE 1028

void* start = NULL;
void* end = NULL;

void* malloc_cust(int bytes) {
    // need to initialize
    if (!start) {
        start = sbrk(0);
        end = start;
    }

    void* current = start;
    // compare the pointers
    while (current != end) {
        int size = *(int*)current;
        // is free and big enough
        if (size < 0 && size < bytes * -1) {
            // move current forward to start of memory
            // we will return
            current -= size;

            *(int*)current = bytes;
            return current + sizeof(int);
        }
        // move forward by size of allocated bytes.
        // will land on number which tells us how many bytes 
        // are allocated
        current += size + sizeof(int);
    }

    // didnt find enough space, allocate more
    end = sbrk(MALLOC_SIZE);

    *(int*)current = bytes;
    return current + sizeof(int); 
}

#undef MALLOC_SIZE