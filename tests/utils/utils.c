#include <stdio.h>  // fprintf
#include <stdlib.h> // abort

/*  The following MUST be manually set using
    malloc.c info to reflect correct values,
*/
struct MallocBlock
{
    size_t max_free;
    size_t sz;
    char   is_free;
    void*  next;
    void*  max_free_ptr;
};

#define BLOCK_META_DATA_SZ \
    sizeof(struct MallocBlock)

#define MY_MALLOC_ALLOC_META \
    (sizeof(size_t) + sizeof(void*))

#define MY_MALLOC_GET_SIZE(VP_META) \
    (*(size_t*)(VP_META))

void check_dupe(char* arr, size_t arr_sz)
{
    for (int j = 0; j != arr_sz; ++j)
    {
        for (int k = j + 1; k != arr_sz; ++k)
        {
            if (arr[j] && arr[j] == arr[k])
            {
                fprintf(stderr, "Duplicate found.\n");
                abort();
            }
        }
    }
}

void check_meta(char* addrs, size_t* vals, size_t index)
{
    size_t alloced_sz = *(size_t*)(addrs[index] - MY_MALLOC_ALLOC_META);
    if (alloced_sz != vals[index] * sizeof(size_t))
    {
        fprintf(stderr, "Different number of bytes.\n");
        abort();
    }

    char* block = *(void**)(addrs[index] - 8); 
    if (!block)
    {
        fprintf(stderr, "Allocation is set to free.\n");
        abort();
    }
}

void check_block(char* addrs, size_t* vals, size_t index, size_t arr_sz)
{
    // get block ptr
    char* curr_ptr = *(void**)(addrs[index] - 8);
    size_t block_sz = *(size_t*)(curr_ptr + 8);
    curr_ptr += BLOCK_META_DATA_SZ;
    // curr_sz should land exactly at end of block
    for (size_t curr_sz = BLOCK_META_DATA_SZ; curr_sz != block_sz;) 
    {
        size_t sz = *(size_t*)curr_ptr; // sz can be 0

        // is the current allocation taken
        if (*(void**)(curr_ptr + 8))
        {
            size_t num_numbers = sz / sizeof(size_t);

            /*  If the meta data for size of the allocation
                is correct, then it will be stored.

                Can have multiple values in "vals" be
                the same. To make sure its the element
                we're looking for check against the address.
            */
            int matched_index = 0;
            for (; matched_index != arr_sz; ++matched_index)
            {
                if
                (
                    vals[matched_index] == num_numbers
                    &&
                    addrs[matched_index] - MY_MALLOC_ALLOC_META == curr_ptr
                )
                {
                    break;
                }
            }

            if (matched_index == arr_sz)
            {
                fprintf(stderr, "Could not find corresponding number of numbers.\n");
                abort();
            }

            curr_ptr += MY_MALLOC_ALLOC_META;
            for (size_t curr_num = 0; curr_num != vals[matched_index]; ++curr_num)
            {
                if (*(size_t*)curr_ptr != vals[matched_index])
                {
                    fprintf(stderr, "Mismatch.\n");
                    abort();
                }

                curr_ptr += sizeof(size_t);
            }

        }
        else
        {
            curr_ptr += MY_MALLOC_ALLOC_META + sz;
        }

        curr_sz += MY_MALLOC_ALLOC_META + sz;
    }
}

void check_all(char* addrs, size_t* vals, size_t arr_sz)
{
    for (int i = 0; i != arr_sz; ++i)
    {
        char* addr = addrs[i];
        if (addr)
        {
            for (size_t j = 0; j != vals[i]; ++j)
            {
                /*  Every element of the array allocated should be
                    equal to the length of the array.
                */
                if (*(size_t*)(addr + (j * sizeof(size_t))) != vals[i])
                {
                    fprintf(stderr, "Mismatch.\n");
                    abort();
                }
            }

            if (*(size_t*)(addr + (vals[i] * sizeof(size_t))) == vals[i])
            {
                /*  This checks that one past the end of the current
                    allocation is not equal to the value stored in
                    the allocation.
                    If one past end is equal, then one of two cases
                        1) Are missing the correct meta data info.
                           Which is clearly an error.
                        2) The meta is correct and just so happen
                           that its value is equal to value stored.
                           This is so unlikely, we ignore this case.
                */
                fprintf(stderr, "Match past end of allocation.\n");
                abort();
            }
        }
    }
}
