/*  Organize the heap into linked list of blocks. Each block is linked list
    with a pointer to largest amount of allocatable memory.

    First fit strategy is used.

    Allocating memory requires searching for first block with enough space
    to hold requested bytes. Then requires searching entire block for to
    set the new largest allocatable number of bytes.

    Requesting time can be as bad a linear in terms of number of blocks,
    but as good as near "constant" if the hints are good.
        NOTE: need a good way to make hints, tree? 
*/

#include <custom_mem/malloc.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h> // sbrk, brk
#include <stdint.h> // SIZE_MAX

// for large allocations, instead of using below bit field macros, use two variables
// like in block to be able to hold entire size

typedef struct MallocInfo
{
    // starting block, gaurenteed to be at start of heap
    struct MallocBlock* start_block;

    // start of of contiguous free bytes, the end of these
    // is the end of heap
    void* start_contiguous;

    // end of heap
    void* end;
}
_malloc_info;

typedef struct MallocBlock
{
    // maximum contiguous number of free allocatable bytes
    size_t max_free;

    // size in bytes of storage this block takes, without block meta data,
    // including allocation meta data
    size_t sz;

    // whether being modified currently
    volatile atomic_char is_free; // 1 free, 0 not free

    // next block
    struct MallocBlock* next;

    // pointer to where max free is
    void* max_free_ptr;
}
_malloc_block;

typedef struct MallocLock
{
    // state of lock
    volatile atomic_char state; // 1 free, 0 in use, 2 error
}
_malloc_lock;

typedef _malloc_block* _mblk;
typedef struct MallocAdjustables _malloc_vars;

#if SIZE_MAX == 0xffffffff
    #define MY_MALLOC_BIT_SIZE 0xffff
    #define MY_MALLOC_BIT_FREE 0x10000
#elif SIZE_MAX == 0xffffffffffffffff
    #define MY_MALLOC_BIT_SIZE 0xffffffff
    #define MY_MALLOC_BIT_FREE 0x100000000
#else
    static_assert(0, "not 32 or 64 bit system");
#endif

// 1 -> free
// 0 -> in use 
#define MY_MALLOC_IS_FREE(VOID_PTR) \
    !(*((size_t*)VOID_PTR) & MY_MALLOC_BIT_FREE)

#define MY_MALLOC_SIZE_GET(VOID_PTR) \
    (*((size_t*)VOID_PTR) & MY_MALLOC_BIT_SIZE)

#define MY_MALLOC_FREE_INUSE(VOID_PTR) \
    *((size_t*)VOID_PTR) |= MY_MALLOC_BIT_FREE

#define MY_MALLOC_FREE_CLEAR(VOID_PTR) \
    *((size_t*)VOID_PTR) &= MY_MALLOC_BIT_SIZE

#define MY_MALLOC_SIZE_SET(VOID_PTR, SIZE) \
    *((size_t*)VOID_PTR) = (*((size_t*)VOID_PTR) & MY_MALLOC_BIT_FREE) + SIZE

#define MY_MALLOC_BLOCK_INITIAL_ALLOC(VOID_PTR) \
    ((char*)VOID_PTR + sizeof(_malloc_block) + sizeof(void*))

static _malloc_info G_state =
{
    .end              = NULL,
    .start_contiguous = NULL,
    .start_block      = NULL,
};

static _malloc_vars G_vars =
{
    .blk_sz   = 1024,
    .more_mem = 4096
};

static _malloc_lock G_lock =
{
    .state = 1
};

// dont wait inside the lock, just say you failed to get

int    _global_lock_get()
{
    char expected = 1;
    if (atomic_compare_exchange_strong(&G_lock.state, &expected, 0))
    {
        return 0;
    }

    // 1) will probably get more memory before swapped
    // 2) will only wait for couple microseconds, can
    //    peg the cpu without "pause" instructions
    while (!G_lock.state);

    return G_lock.state;
}

void   _global_lock_free()
{
    atomic_store(&G_lock.state, 1);
}

int    _mem_get_more()
{
    // expand heap by some number of bytes
    
    if (brk((char*)G_state.end + G_vars.more_mem)) // set errno
    {
        return -1;
    }

    G_state.end = (char*)G_state.end + G_vars.more_mem;

    return 0;
}

void*  _mem_init()
{
    // initialization for heap

    void* res = sbrk(0);
    if (G_state.end == (void*)-1) // set errno
    {
        return NULL;
    }

    return res;
}

size_t _block_det_sz_no_meta(size_t bytes)
{
    // determine the total number of bytes a block will
    // take up, without meta data
    
    if (bytes < G_vars.blk_sz)
    {
        return G_vars.blk_sz;
    }

    // for gcc and clang
    return 1 << (__builtin_clzl(bytes) + 1);
}

size_t _block_det_sz(size_t bytes)
{
    // determine the total number of bytes a block will
    // take up

    return _block_det_sz_no_meta(bytes) + sizeof(_malloc_block);
}

void*  _block_alloc_unsafe(_malloc_block* block, size_t bytes)
{
    // allocate bytes from block assuming bytes < block.max_free and
    // update the largest possible allocation in block
    // return the start of allocation

    char expected = 1;
    if (atomic_compare_exchange_strong(&block->is_free, &expected, 0))
    {
        void* to_ret = block->max_free_ptr;

        MY_MALLOC_FREE_INUSE(block->max_free_ptr);
        size_t remaining = block->max_free - bytes;

        if (remaining <= 64) // should be greater than fast cache
        {
            // no more room in block

            MY_MALLOC_SIZE_SET(block->max_free_ptr, block->max_free);

            block->max_free = 0;
            block->max_free_ptr = NULL;

            return to_ret;
        }

        // add another space for potential allocation

        MY_MALLOC_SIZE_SET(block->max_free_ptr, bytes);

        void* after_insert = (char*)block->max_free_ptr + bytes + sizeof(void*);
        MY_MALLOC_FREE_CLEAR(after_insert);
        MY_MALLOC_SIZE_SET(after_insert, remaining);

        // get new largest allocatable size for block

        void* curr = (void*)(block + 1);
        size_t i = 0;
        while (i < block->sz)
        {
            size_t curr_size = MY_MALLOC_SIZE_GET(curr); 
            if (MY_MALLOC_IS_FREE(curr) && curr_size > MY_MALLOC_SIZE_GET(after_insert))
            {
                after_insert = curr;
            }

            i += curr_size + sizeof(void*);
            curr = (char*)curr + curr_size + sizeof(void*);
        }
        block->max_free_ptr = after_insert;
        block->max_free = MY_MALLOC_SIZE_GET(after_insert);

        block->is_free = 1;

        return to_ret;
    }

    return NULL;
}

size_t _block_create(size_t bytes, void* where)
{
    // create a block, allocating bytes into it, adding the block
    // on where
    // return the total number of bytes taken

    // Note: we assume "bytes" is allocated in the first available
    //       space in the block outside this function

    void* ptr = where;

    size_t block_size = _block_det_sz_no_meta(bytes);

    _malloc_block new_block =
    {
        .sz           = block_size,
        .is_free      = 1,
        .next         = NULL
    };
    _malloc_block* block = ptr;
    *block = new_block;

    ptr = (char*)ptr + sizeof(_malloc_block);

    MY_MALLOC_FREE_INUSE(ptr);
    MY_MALLOC_SIZE_SET(ptr, bytes);

    ptr = (char*)ptr + bytes + sizeof(void*);

    size_t max_free = block_size - bytes - sizeof(void*);
    MY_MALLOC_FREE_CLEAR(ptr);
    MY_MALLOC_SIZE_SET(ptr, max_free);
    block->max_free_ptr = ptr;
    block->max_free = max_free;

    return ((char*)ptr - (char*)where) + max_free;
}

_mblk  _block_get_hard(size_t bytes)
{
    // try to get a block with enough bytes, will try
    // really hard to find a block

    return NULL;

}

_mblk  _block_get_easy(size_t bytes)
{
    // try to get a block with enough bytes, if fails
    // to do so, just give up

    _malloc_block* block = G_state.start_block;

    while (block && bytes > block->max_free)
    {
        block = block->next;
    }

    return block;
}

// need to figure out if mmap and brk can work together

void*  _initialize(size_t bytes)
{
    // set up the heap and head of linked list

    void* heap_start = _mem_init();
    if (!heap_start)
    {
        return NULL;
    }
    
    G_state.end = heap_start;
    if (_mem_get_more())
    {
        return NULL;
    }

    size_t taken = _block_create(bytes, heap_start);
    G_state.start_contiguous = (char*)heap_start + taken;
    
    G_state.start_block = (_malloc_block*)heap_start;

    _global_lock_free();

    return MY_MALLOC_BLOCK_INITIAL_ALLOC(heap_start);
}

void*  _create_allocate(size_t bytes, _malloc_block* block)
{
    // create a block onto the given block, which should be
    // the end block and allocate bytes into it
    // return pointer to allocated space

    void* old_start = G_state.start_contiguous;

    size_t blk_sz = _block_create(bytes, G_state.start_contiguous);
    block->next = (_malloc_block*)old_start; // race cond?

    G_state.start_contiguous = (char*)G_state.start_contiguous + blk_sz;

    return MY_MALLOC_BLOCK_INITIAL_ALLOC(old_start); 

}

void*  my_malloc(size_t bytes)
{
    // allocate bytes somewhere on the heap
    // return pointer to allocated space 

    // try a simple allocation
    _malloc_block* block = _block_get_easy(bytes);
    if (block)
    {
        void* alloc = _block_alloc_unsafe(block, bytes);
        if (alloc)
        {
            return alloc;
        }
    }

    if (!block)
    {
        return _initialize(bytes);
    }

    if (_block_det_sz(bytes) < (char*)G_state.end - (char*)G_state.start_contiguous)
    {
        return _create_allocate(bytes, block);
    }

    // only want to globally lock when adding more memory  / initialzing
    // otherwise, just lock the last block, that way no threads can make more
    // blocks

    // need to figure out if mmap is needed outside the block making functions
    // that not their job

    if (_mem_get_more())
    {
        return NULL;
    }

    return _create_allocate(bytes, block);

}

void my_free(void* ptr)
{

}
