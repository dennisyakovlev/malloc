/*  Organize the heap into linked list of mmap'ed mappings. Each mapping has a
    linked list of blocks. Each block is a linked list of allocations.

    An allocation causes a searching all three levels of linked lists. A first fit
    strategy is used for allocations. Once a space is found in a block, the space
    gets set to taken and the entire block is interated over to update meta data.

    Freeing memory sets the previously allocated space to free, without updating
    the block metadata. The metadata can only be updated when another allocation
    is put in the block.
*/

#include <custom_mem/malloc.h>
#include <stdatomic.h>
#include <assert.h>
#include <sys/mman.h> // mmap
#include <unistd.h>   // sbrk, brk
#include <stdint.h>   // SIZE_MAX

// for large allocations, instead of using below bit field macros, use two variables
// like in block to be able to hold entire size

typedef struct MallocGlobal
{
    struct MallocMapping* start_map;
}
_global;

typedef struct MallocMapping
{
    // starting block in this mapping
    void* start_block;

    // ending block in this mapping
    void* end_block;

    // next mapping
    struct MallocMapping* next;

    // start of this mapping
    void* start;

    // end of this mapping
    void* end;
}
_mapping;

typedef struct MallocBlock
{
    // maximum contiguous number of free allocatable bytes
    size_t max_free;

    // size in bytes of storage this block takes
    size_t sz;

    // whether being modified currently
    volatile atomic_char is_free;

    // next block
    void* next;

    // pointer to metadata of where max free space is
    void* max_free_ptr;
}
_block;

typedef struct MallocLock
{
    // state of lock
    volatile atomic_char state; // 1 free, 0 in use, 2 error
}
_lock;

typedef _block* _blk;
typedef _mapping* _map;
typedef struct MallocAdjustables _vars;

#if SIZE_MAX == 0xffffffff
    #define MY_MALLOC_SHIFTER 0x80000000
#elif SIZE_MAX == 0xffffffffffffffff
    #define MY_MALLOC_SHIFTER 0x8000000000000000
#else
    static_assert(0, "not 32 or 64 bit system");
#endif

#if !(defined(__clang__) || defined(__GNUC__) || defined(__GNUG__))
    static_assert(0, "not gcc or clang")
#endif

// meta data is 
// size_t - size of this allocation
// +
// void* - start of block meta data

// how many bytes do i want my block to take
#define MY_MALLOC_BLOCK_EXPANSION(sz) \
    (((sz) | 1024) + sizeof(_block) + MY_MALLOC_ALLOC_META)

/* meta data per allocation

   meta data is 
   size_t - size of this allocation
   +
   void* - start of block meta data which
           allocation is in
*/
#define MY_MALLOC_ALLOC_META \
    (sizeof(size_t) + sizeof(void*))

// get size of allocation
#define T_MY_MALLOC_GET_SIZE(VP_META) \
    (*(size_t*)(VP_META))

// get whether allocation is free
// NULL -> free
// else -> not free, start of block
#define T_MY_MALLOC_GET_AVAILABILITY(VP_META) \
    ((char*)(VP_META) + sizeof(size_t))

// set size of allocation
#define T_MY_MALLOC_SET_SIZE(VP_META, SZ) \
    *(size_t*)(VP_META) = SZ

// set allocation to free
#define T_MY_MALLOC_SET_FREE(VP_META) \
    do \
    { \
        void* temp = T_MY_MALLOC_GET_AVAILABILITY(VP_META); \
        temp = NULL; \
        if (temp); \
    } while (0)
    

// set allocation to inuse
#define T_MY_MALLOC_SET_INUSE(VP_META, VP_BLOCK) \
    do \
    { \
        void* temp = T_MY_MALLOC_GET_AVAILABILITY(VP_META); \
        temp = VP_BLOCK; \
        if (temp); \
    } while (0)

// move to start of meta data for next allocation
#define T_MY_MALLOC_NEXT(VP_META) \
    ((char*)(VP_META) + MY_MALLOC_ALLOC_META + T_MY_MALLOC_GET_SIZE(VP_META))

static _global G_global =
{
    .start_map = NULL
};

static _vars G_vars =
{
    .more_mem = 1048576,
    .cache_sz = 64
};

static _lock G_lock =
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

void*  _mem_get(size_t bytes)
{
    // get bytes more memory

    void* res = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (res == (void*)-1) // set errno
    {
        return NULL;
    }

    return res;
}

size_t _mem_more_sz(size_t bytes)
{
    // determine number of new bytes which will be allocated
    
    // Note: function will return 1 if bytes has the highest
    //       bit set on the platform
    //       ie: on 32 bit if bytes >= 2^31
    //           on 64 but if bytes >= 2^63  

    if (bytes < G_vars.more_mem)
    {
        return G_vars.more_mem;
    }

    // lowest power of 2 greater than bytes
    return MY_MALLOC_SHIFTER >> (__builtin_clzl(bytes) - 1);
}

int    _block_has_room(size_t bytes, _block* block)
{
    // whether block has enough room for bytes
    // return 1 is yes, 0 otherwise

    const size_t free = block->max_free;
    if (free <= MY_MALLOC_ALLOC_META) {
        return 0;
    }

    return block->max_free - MY_MALLOC_ALLOC_META >= bytes;
}

void   _block_update_meta(void* block)
{
    // set new largest allocatable size for block
    _block* block_ptr = block;

    void* curr = (char*)block + sizeof(_block), *max = curr;
    size_t i = 0;
    while (i < block_ptr->sz)
    {
        size_t curr_size = T_MY_MALLOC_GET_SIZE(curr);
        if (T_MY_MALLOC_GET_AVAILABILITY(curr) && curr_size > T_MY_MALLOC_GET_SIZE(max))
        {
            max = curr;
        }

        i += curr_size + MY_MALLOC_ALLOC_META;
        curr = T_MY_MALLOC_NEXT(curr);
    }
    block_ptr->max_free_ptr = max;
    block_ptr->max_free = T_MY_MALLOC_GET_SIZE(max);
}

void*  _block_alloc_unsafe(size_t bytes, void* block)
{
    // allocate bytes from block and update the largest possible
    // allocation in block
    // return the start of allocation

    // Assume: bytes < block.max_free - ALLOC_META

    _block* block_ptr = block;

    void* to_ret = (char*)block_ptr->max_free_ptr + MY_MALLOC_ALLOC_META;

    T_MY_MALLOC_SET_INUSE(block_ptr->max_free_ptr, block);
    size_t remaining = block_ptr->max_free - bytes - MY_MALLOC_ALLOC_META;

    T_MY_MALLOC_SET_SIZE(block_ptr->max_free_ptr, bytes);

    if (remaining <= G_vars.cache_sz) // should be greater than fast cache
    {
        // no more room in block
        block_ptr->max_free = 0;
        block_ptr->max_free_ptr = NULL;

        return to_ret;
    }

    // add another space for potential allocation
    void* after_insert = (char*)block_ptr->max_free_ptr + bytes + MY_MALLOC_ALLOC_META;
    T_MY_MALLOC_SET_FREE(after_insert);
    T_MY_MALLOC_SET_SIZE(after_insert, remaining);

    _block_update_meta(block);

    return to_ret;
}

void   _block_create_unsafe(size_t sz, void* where)
{
    // create a block of sz starting the block on where
    // return the total number of bytes taken

    _block new_block =
    {
        .sz           = sz,
        .is_free      = 1,
        .next         = NULL,
        .max_free_ptr = (char*)where + sizeof(_block),
        .max_free     = sz - sizeof(_block)
    };
    *(_block*)where = new_block;
}

void*  _block_get(size_t bytes, _mapping** mapping)
{
    // try to get a block with enough bytes
    // return block if found, otherwise null

    // Note: never want *mapping to be set to NULL
    //       unless is was passed as null

    if (!(*mapping))
    {
        return NULL;
    }

    void* block = (*mapping)->start_block;
    while (block)
    {
        if (_block_has_room(bytes, block))
        {
            return block;
        }

        block = ((_block*)block)->next;
    }

    while((*mapping)->next)
    {
        while (block)
        {
            if (_block_has_room(bytes, block))
            {
                return block;
            }

            block = ((_block*)block)->next;
        }

        *mapping = (*mapping)->next;
    }        

    return NULL;
}

_map   _mapping_create_unsafe(size_t sz)
{
    // create mapping capable of holding sz
    // return where the mapping is created

    size_t more_mem = _mem_more_sz(sz + sizeof(_mapping));
    void* start = _mem_get(more_mem);

    _mapping new_mapping =
    {
        .start       = start,
        .end         = (char*)start + more_mem,
        .start_block = NULL,
        .end_block   = NULL, 
        .next        = NULL
    };
    _mapping* mapping = start;
    *mapping = new_mapping;

    return start;
}

void*  _mapping_create(size_t bytes, _mapping** mapping)
{
    // request a mapping with bytes allocated onto it
    // return the start of allocation

    size_t block_sz = MY_MALLOC_BLOCK_EXPANSION(bytes);
    void* new_mapping = _mapping_create_unsafe(block_sz + sizeof(_mapping));
    *mapping = new_mapping;

    void* where = (char*)new_mapping + sizeof(_mapping);

    _block_create_unsafe(block_sz, where);

    (*mapping)->start_block = where;
    (*mapping)->end_block = where;

    return _block_alloc_unsafe(bytes, where);
}

void*  _mapping_block_create(size_t bytes, _mapping** mapping)
{
    // add block onto existing mapping if can, otherwise create
    // new mapping
    // return start of allocation

    _block* end_block = (*mapping)->end_block;
    void* where = (char*)(*mapping)->end_block + sizeof(_block) + end_block->sz;
    size_t block_sz = MY_MALLOC_BLOCK_EXPANSION(bytes);

    if (block_sz > (char*)(*mapping)->end - (char*)where)
    {
        return _mapping_create(bytes, &(*mapping)->next);
    }

    _block_create_unsafe(block_sz, where);

    (*mapping)->end_block = where;
    end_block->next = where;

    return _block_alloc_unsafe(bytes, where);
}

// currently get working with single threaded

void*  my_malloc(size_t bytes)
{
    // allocate bytes somewhere on the heap
    // return pointer to allocated space 

    _mapping* mapping = G_global.start_map;
    void* block = _block_get(bytes, &mapping);
    
    if (block)
    {
        return _block_alloc_unsafe(bytes, block);
    }

    if (!mapping)
    {
        return _mapping_create(bytes, &G_global.start_map);
    }

    return _mapping_block_create(bytes, &mapping);
}

void my_free(void* ptr)
{
    // free usage in block
    // get pointer to start of block
    // call _block_update_meta
}
