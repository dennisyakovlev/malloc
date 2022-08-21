#pragma once
#include <stddef.h>

/*  Extra features
        1) fragmentation clean up
                each time malloc or free is called, some fragmentation cleanup happens
        2) hints
                keep hints, speeds up allocation but takes more memory
                - maybe like array of pointers to open spaces for different sizes
*/

/*  To improve efficient, can have a ds in MallocInfo which contains pointers
    to all our blocks, then we can allocate based on that, and rearrange
    the pointer to next inside the blocks as necessary when modifying the ds.
*/

// have a fast cache for allocation less than X bytes, and to find which is free
// use what we did with old malloc and bits to represent which are and arent taken
//  use asm instruction to find first 1, which represent free?

struct MallocAdjustables
{
    // minimum amount of more memory to request at a time
    size_t more_mem;

    // 
    size_t cache_sz;
};

void* my_malloc(size_t);

void my_free(void*);
