/*  Simple malloc implementation.

    Allocation idea is to have small blocks. who's size is decided based
    on the number of bits in a void* times mini block size. Then every bit
    in the void* represents whether a piece of memory is free. To be able
    to free memory, need a bit after each allocated space which indicates
    the end of a used space.

    Fatal Flaw
        The above idea is essentially the same as just having some info
        at the start of every allocation. The overhead is same since
        a bit corresponds to a mini block (8 bytes) which is probably a
        void* (for 64 bit). But current coded version is much less
        readable.

    Pitfalls
        - if requested number of bytes is greater than MALLOC_SIZE, will fail
          ungracefully
        - when runs out of memory, fails ungracefully
        - doesn't work with mmap since operates on program break
        - there's no free, realloc, calloc functions to go with malloc
        - extra memory usage if requested size if < SMALL_BLOCK_SIZE
        - not portable because of uintptr_t
        - no way to free memory
    Strengths
        - by virtue of lack of features, no fragmentation (again)
        - only has overhead of 1 sizeof(void*) per SMALL_BLOCK_SIZE

*/
#include <custom_mem/malloc.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>

void* start = NULL;
void* end = NULL;
size_t num_blocks = 0;

/*  idea is for allocations < 512, just throw them 
    inside of a 512 block, then for allocations < 4096, put them,
    in own thing

    so we have blocks of size 512, and 4096.

    at the very start of each block, is a pointer to the start
    of the next block of the same type
        if the block is the last block of the types, then starting
        pointer points to end of this block
    then for every allocation within the block, is pointer to first
    empty allocation within the block 

*/


// the smallest block size used, mapped to one bit 
#define MINI_BLOCK_SIZE 8
#define VOID_POINTER_BITS (sizeof(void*) * CHAR_BIT) 
// the block size which stores mini blocks
#define SMALL_BLOCK_SIZE (MINI_BLOCK_SIZE * VOID_POINTER_BITS)
#define MALlOC_SIZE 4096

/*  0 indicates taken, 1 indicates free
*/

void* malloc_cust(size_t bytes) {
    if (!start) {
        start = sbrk(MALlOC_SIZE);
        *(uintptr_t*)start = ~0;
        end = sbrk(0);
        num_blocks = (end - start) / SMALL_BLOCK_SIZE;
    }

    void* current = start;
    size_t rem_blocks = num_blocks;
    while (rem_blocks--) {
        /*  The number which is needed to be able to say there is
            enough free space for requested bytes.

            ((bytes / MINI_BLOCK_SIZE) + 1) is the number of mini
            blocks needed to hold bytes.
            Then set the first #of_mini_block bits to 1.
        */
        unsigned int target = ~(~0 << ((bytes / MINI_BLOCK_SIZE) + 1));
        /*  Needs to have same behaviour as void* (bits, endian) since
            could possibly need 
        */
        uintptr_t i = 0;
        unsigned char j = 0;
        uintptr_t info = *(uintptr_t*)current;
        while (info) {
            /*  Shift i over 1, multiply by least significant bit
                of info and add one. Gives effect of cumulatively
                settings first n bits to 1.
            */
            i = (i << 1) * (info & 1) + 1;
            if (i == target) {
                // set bits to 0, signify taken

                // j shifts wrong amount, for 27 bytes, havd 
                // 1...11110000111
                // should subtract number of 0's long minus 1 from j. ie
                // i << (j - (bytes / MINI_BLOCK_SIZE))
                unsigned char j_shift = bytes / MINI_BLOCK_SIZE;
                *(uintptr_t*)current = ~(i << (j - j_shift)) & *(uintptr_t*)current;
                return current + MINI_BLOCK_SIZE * j_shift;
            }
            info = info >> 1;
            ++j;
        }
        current += SMALL_BLOCK_SIZE + sizeof(void*);
    }

    /*  Didn't find any space in a small block which can hold the
        requested number of bytes. Request atleast another page.
    */
    end = sbrk(MALlOC_SIZE);

}

#undef MINI_BLOCK_SIZE
#undef VOID_POINTER_BITS 
#undef SMALL_BLOCK_SIZE
#undef PAGE_SIZE