// do a very thourough inspection using the same idea
// as mix.c
// this test relies on internal aspects of the code

#include <custom_mem/malloc.h>
#include <stdlib.h> // abort
#include <stdio.h>  // stderr, fprintf, size_t

#define NUM_CALLS 1024

static int    indicies[NUM_CALLS] = {36, 23, 58, 3, 62, 8, 27, 33, 54, 47, 17, 63, 64, 9, 54, 5, 5, 62, 62, 51, 32, 54, 20, 41, 56, 64, 63, 47, 53, 55, 40, 31, 4, 49, 20, 61, 11, 44, 11, 10, 43, 46, 17, 46, 34, 27, 7, 56, 26, 47, 53, 34, 64, 53, 17, 21, 52, 22, 27, 52, 14, 0, 53, 6, 57, 22, 19, 31, 2, 46, 8, 45, 3, 25, 51, 17, 10, 58, 60, 49, 53, 24, 64, 54, 20, 60, 45, 43, 30, 23, 10, 18, 4, 51, 3, 50, 16, 22, 18, 48, 55, 36, 38, 56, 47, 55, 39, 11, 25, 34, 20, 29, 33, 51, 50, 30, 57, 36, 26, 21, 34, 2, 49, 29, 32, 17, 13, 22, 37, 23, 6, 37, 17, 26, 62, 4, 46, 4, 31, 15, 43, 15, 25, 59, 57, 60, 14, 50, 42, 33, 48, 6, 33, 4, 64, 19, 19, 34, 38, 59, 19, 23, 0, 59, 6, 38, 9, 57, 14, 31, 10, 53, 34, 27, 38, 59, 35, 27, 47, 52, 46, 51, 6, 50, 22, 55, 30, 21, 50, 33, 33, 30, 24, 43, 34, 7, 38, 41, 33, 26, 25, 53, 1, 2, 8, 34, 43, 28, 56, 8, 53, 56, 44, 47, 40, 46, 38, 8, 56, 3, 19, 44, 24, 19, 58, 2, 5, 22, 6, 39, 33, 40, 29, 47, 16, 44, 14, 49, 22, 59, 7, 3, 45, 16, 47, 50, 60, 58, 52, 50, 9, 24, 0, 60, 17, 20, 0, 45, 7, 61, 46, 53, 52, 15, 18, 30, 31, 18, 30, 40, 27, 15, 1, 1, 38, 15, 6, 6, 3, 0, 48, 20, 45, 17, 47, 28, 41, 2, 31, 59, 54, 48, 0, 59, 45, 48, 50, 18, 24, 50, 31, 20, 29, 15, 21, 10, 24, 15, 45, 15, 41, 46, 36, 47, 50, 19, 53, 54, 38, 36, 49, 29, 2, 23, 26, 57, 27, 36, 49, 34, 64, 43, 30, 25, 51, 26, 61, 14, 61, 64, 17, 59, 9, 64, 60, 7, 23, 30, 26, 14, 39, 18, 58, 61, 60, 30, 15, 13, 43, 36, 41, 55, 29, 52, 43, 0, 29, 12, 43, 20, 53, 8, 17, 47, 16, 21, 41, 28, 28, 19, 57, 31, 54, 16, 42, 7, 39, 25, 17, 31, 29, 29, 56, 19, 58, 9, 35, 11, 6, 64, 20, 58, 2, 29, 60, 12, 19, 15, 54, 35, 37, 41, 1, 53, 36, 11, 29, 17, 31, 29, 1, 52, 35, 13, 56, 41, 59, 24, 9, 60, 19, 40, 41, 53, 8, 19, 8, 11, 17, 58, 23, 16, 5, 7, 63, 6, 22, 53, 37, 1, 27, 61, 19, 46, 9, 14, 25, 9, 18, 23, 13, 24, 28, 46, 63, 30, 48, 5, 63, 23, 44, 7, 3, 56, 28, 35, 9, 64, 7, 13, 6, 2, 15, 32, 58, 5, 28, 5, 45, 13, 34, 59, 44, 50, 7, 40, 49, 10, 27, 36, 31, 36, 28, 24, 64, 64, 23, 38, 6, 22, 59, 24, 64, 43, 8, 1, 22, 63, 1, 8, 28, 17, 56, 20, 58, 58, 19, 52, 38, 35, 6, 49, 7, 43, 30, 29, 7, 38, 10, 58, 33, 59, 14, 22, 45, 4, 40, 15, 26, 45, 24, 11, 29, 4, 34, 8, 41, 36, 2, 59, 37, 42, 13, 63, 62, 20, 45, 44, 51, 64, 38, 36, 25, 25, 38, 62, 4, 3, 4, 38, 27, 43, 20, 6, 14, 64, 20, 20, 42, 19, 10, 17, 51, 55, 12, 33, 39, 31, 27, 13, 36, 28, 13, 41, 14, 26, 9, 42, 4, 58, 7, 33, 18, 4, 41, 31, 57, 35, 13, 28, 34, 8, 23, 61, 23, 30, 10, 14, 57, 33, 45, 6, 46, 15, 31, 57, 50, 13, 35, 47, 24, 46, 32, 8, 16, 58, 60, 43, 19, 42, 13, 59, 44, 46, 38, 10, 38, 34, 1, 36, 26, 28, 15, 57, 64, 36, 43, 20, 61, 11, 17, 61, 6, 24, 31, 4, 58, 24, 53, 53, 35, 64, 38, 4, 26, 43, 2, 6, 44, 42, 30, 31, 55, 23, 8, 9, 62, 10, 27, 50, 43, 3, 4, 11, 50, 28, 55, 8, 40, 42, 20, 41, 47, 8, 19, 25, 21, 58, 43, 61, 57, 17, 48, 2, 15, 23, 34, 41, 23, 7, 49, 19, 58, 8, 57, 39, 11, 63, 54, 36, 33, 57, 56, 62, 16, 53, 14, 33, 23, 63, 43, 14, 56, 10, 29, 51, 5, 41, 17, 36, 20, 36, 62, 13, 3, 63, 38, 0, 49, 47, 20, 64, 0, 43, 39, 35, 61, 0, 60, 29, 21, 56, 57, 50, 23, 58, 3, 25, 17, 58, 45, 46, 57, 18, 31, 5, 57, 38, 55, 38, 55, 36, 10, 64, 25, 43, 50, 57, 23, 54, 30, 10, 7, 29, 48, 16, 35, 54, 46, 3, 27, 37, 2, 2, 25, 53, 38, 0, 32, 61, 37, 36, 60, 56, 23, 62, 16, 44, 53, 17, 49, 12, 22, 26, 53, 26, 21, 0, 37, 13, 26, 38, 57, 19, 17, 30, 10, 39, 3, 1, 9, 16, 30, 55, 8, 57, 29, 58, 26, 28, 9, 17, 63, 31, 39, 17, 30, 62, 36, 32, 24, 16, 28, 11, 13, 32, 20, 6, 24, 56, 29, 13, 37, 54, 30, 22, 18, 61, 33, 55, 26, 22, 17, 11, 42, 1, 64, 50, 46, 45, 59, 24, 61, 59, 44, 36, 52, 2, 35, 51, 28, 12, 42, 14, 15, 16, 63, 31, 3, 22, 7, 26, 30, 30, 43, 58, 34, 60, 63, 38, 31, 8, 8, 63, 0, 24, 25, 2, 12, 31, 22, 26, 33, 47, 2, 38, 54, 22, 58, 40, 46, 35, 57, 61, 20, 34, 17, 50, 35, 61, 14, 3, 18, 39, 49, 10, 49, 9, 16, 26, 44, 51, 45, 24, 5, 32, 31, 12, 9, 2, 42, 0, 63, 36, 43, 13, 32, 18, 2, 46, 46, 1, 37, 2, 50, 64, 17, 53, 16, 27, 10, 33, 20, 45, 58, 10, 14, 61, 57, 7, 53, 9, 4, 14};
static char*  addresses[65]; // numbers allocated at i
static size_t number_at[65]; // number of allocated numbers at i

void check_dupe()
{
    /*  Check for duplicate returned addresses.
    */

    for (int j = 0; j != 65; ++j)
    {
        for (int k = j + 1; k != 65; ++k)
        {
            if (addresses[j] && addresses[j] == addresses[k])
            {
                fprintf(stderr, "Duplicate found.\n");
                abort();
            }
        }
    }
}

void check_meta(int index)
{
    /*  Check meta data for this particular allocation.

        Meta data consists of (size_t,void*) in this order.
            size_t - size of allocation
            void*  - start of block, NULL if free
    */

    size_t alloced_sz = *(size_t*)(addresses[index] - 16);
    if (alloced_sz != number_at[index] * sizeof(size_t))
    {
        fprintf(stderr, "Different number of bytes.\n");
        abort();
    }

    char* block = *(void**)(addresses[index] - 8); 
    if (!block)
    {
        fprintf(stderr, "Allocation is set to free.\n");
        abort();
    }
}

void check_block(int index)
{
    /*  Check the block which is associated with the address
        at index is still intact and correct.
        ie
            Every allocation contains the correct number of
            numbers set to the correct value, and the meta
            data is correct for each allocation.
            The block is still the correct size according
            to meta data.
    */
    
    char* curr_ptr = *(void**)(addresses[index] - 8);
    size_t block_sz = *(size_t*)(curr_ptr + 8);
    curr_ptr += 40;
    // curr_sz should land exactly at end of block
    for (size_t curr_sz = 40; curr_sz != block_sz;) 
    {
        size_t sz = *(size_t*)curr_ptr; // sz can be 0

        // is the current allocation taken
        if (*(void**)(curr_ptr + 8))
        {
            size_t num_numbers = sz / sizeof(size_t);

            /*  If the meta data for size of the allocation
                is correct, then it will be stored.

                Can have multiple values in "number_at" be
                the same. To make sure its the element
                we're looking for check against the address.
            */
            int matched_index = 0;
            for (; matched_index != 65; ++matched_index)
            {
                if
                (
                    number_at[matched_index] == num_numbers
                    &&
                    addresses[matched_index] - 16 == curr_ptr
                )
                {
                    break;
                }
            }

            if (matched_index == 65)
            {
                fprintf(stderr, "Could not find corresponding number of numbers.\n");
                abort();
            }

            curr_ptr += 16;
            for (size_t curr_num = 0; curr_num != number_at[matched_index]; ++curr_num)
            {
                if (*(size_t*)curr_ptr != number_at[matched_index])
                {
                    fprintf(stderr, "Mismatch.\n");
                    abort();
                }

                curr_ptr += sizeof(size_t);
            }

        }
        else
        {
            curr_ptr += 16 + sz;
        }

        curr_sz += 16 + sz;
    }
}

void check_all()
{
    for (int i = 0; i != 65; ++i)
    {
        char* addr = addresses[i];
        if (addr)
        {
            for (size_t j = 0; j != number_at[i]; ++j)
            {
                if (*(size_t*)(addr + (j * sizeof(size_t))) != number_at[i])
                {
                    fprintf(stderr, "Mismatch.\n");
                    abort();
                }
            }

            if (*(size_t*)(addr + (number_at[i] * sizeof(size_t))) == number_at[i])
            {
                fprintf(stderr, "Match.\n");
                abort();
            }
        }
    }
}

/*  Need to check whether the functions in utils are correct
*/

int main(int argc, char const *argv[])
{
    for (int i = 0; i != NUM_CALLS; ++i)
    {
        check_dupe(i);        

        const int index = indicies[i];

        if (addresses[index])
        {
            my_free(addresses[index]);

            addresses[index] = NULL;
            number_at[index] = 0;
        }

        size_t num_numbers = 0; // how many numbers do we want
        for (int j = 0; j != index && i + j < NUM_CALLS; ++j)
        {
            num_numbers += indicies[i + j];
        }

        size_t req_bytes = num_numbers * sizeof(size_t);
        void* res = my_malloc(req_bytes);

        addresses[index] = res;
        number_at[index] = num_numbers;

        // set each value to number of numbers
        for (size_t j = 0; j != num_numbers; ++j)
        {
            *((size_t*)res + j) = num_numbers;
        }

        check_meta(index);
        check_block(index);

        check_all();
    }

    return 0;
    
}
