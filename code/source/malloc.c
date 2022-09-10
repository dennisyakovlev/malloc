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
    struct MallocBlock* start_block;

    // ending block in this mapping
    struct MallocBlock* end_block;

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
    struct MallocBlock* next;

    // pointer to where max free is
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
    #define MY_MALLOC_BIT_SIZE 0xffff
    #define MY_MALLOC_BIT_FREE 0x10000
    #define MY_MALLOC_SHIFTER 0x80000000
#elif SIZE_MAX == 0xffffffffffffffff
    #define MY_MALLOC_BIT_SIZE 0xffffffff
    #define MY_MALLOC_BIT_FREE 0x100000000
    #define MY_MALLOC_SHIFTER 0x8000000000000000
#else
    static_assert(0, "not 32 or 64 bit system");
#endif

#if !(defined(__clang__) || defined(__GNUC__) || defined(__GNUG__))
    static_assert(0, "not gcc or clang")
#endif

// 1 -> free
// 0 -> in use 
#define MY_MALLOC_IS_FREE(VOID_PTR) \
    !(*((size_t*)(VOID_PTR)) & MY_MALLOC_BIT_FREE)

#define MY_MALLOC_SIZE_GET(VOID_PTR) \
    (*((size_t*)(VOID_PTR)) & MY_MALLOC_BIT_SIZE)

#define MY_MALLOC_FREE_INUSE(VOID_PTR) \
    *((size_t*)(VOID_PTR)) |= MY_MALLOC_BIT_FREE

#define MY_MALLOC_FREE_CLEAR(VOID_PTR) \
    *((size_t*)(VOID_PTR)) &= MY_MALLOC_BIT_SIZE

#define MY_MALLOC_SIZE_SET(VOID_PTR, SIZE) \
    *((size_t*)(VOID_PTR)) = (*((size_t*)VOID_PTR) & MY_MALLOC_BIT_FREE) + (SIZE)

// meta data per allocation
#define MY_MALLOC_ALLOC_META (sizeof(size_t))

// how many bytes do i want my block to take
#define MY_MALLOC_BLOCK_EXPANSION(sz) \
    (((sz) | 1024) + sizeof(_block) + MY_MALLOC_ALLOC_META)

static _global G_global =
{
    .start_map = NULL
};

static _vars G_vars =
{
    .more_mem = 1048576,
    .cache_sz = 1024
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
    // return 0 is yes, 1 otherwise

    return block->max_free - MY_MALLOC_ALLOC_META >= bytes;
}

void*  _block_alloc_unsafe(_block* block, size_t bytes)
{
    // allocate bytes from block and update the largest possible
    // allocation in block
    // return the start of allocation

    // Assume: bytes < block.max_free - sizeof(size_t)

    void* to_ret = (char*)block->max_free_ptr + MY_MALLOC_ALLOC_META;

    MY_MALLOC_FREE_INUSE(block->max_free_ptr);
    size_t remaining = block->max_free - bytes - MY_MALLOC_ALLOC_META;

    MY_MALLOC_SIZE_SET(block->max_free_ptr, bytes);

    if (remaining <= 64) // should be greater than fast cache
    {
        // no more room in block
        block->max_free = 0;
        block->max_free_ptr = NULL;

        return to_ret;
    }

    // add another space for potential allocation
    void* after_insert = (char*)block->max_free_ptr + bytes + MY_MALLOC_ALLOC_META;
    MY_MALLOC_FREE_CLEAR(after_insert);
    MY_MALLOC_SIZE_SET(after_insert, remaining);

    // get new largest allocatable size for block

    void* curr = block + 1;
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

_blk   _block_get(size_t bytes, _mapping** mapping)
{
    // try to get a block with enough bytes
    // return block if found, otherwise null

    // Note: never want *mapping to be set to NULL
    //       unless is was passed as null

    if (!(*mapping))
    {
        return NULL;
    }

    _block* block = (*mapping)->start_block;
    while (block)
    {
        if (!_block_has_room(bytes, block))
        {
            return block;
        }

        block = block->next;
    }

    while((*mapping)->next)
    {
        while (block)
        {
            if (!_block_has_room(bytes, block))
            {
                return block;
            }

            block = block->next;
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
    *(_mapping*)start = new_mapping;

    return start;
}

void*  _mapping_create(size_t bytes, _mapping** mapping)
{
    // request a mapping with bytes allocated onto it
    // return the start of allocation

    size_t block_sz = MY_MALLOC_BLOCK_EXPANSION(bytes);
    *mapping = _mapping_create_unsafe(block_sz + sizeof(_mapping));

    void* where = *mapping + 1;
    _block_create_unsafe(block_sz, where);

    (*mapping)->start_block = (_block*)where;
    (*mapping)->end_block = (_block*)where;

    return _block_alloc_unsafe((_block*)where, bytes);
}

void*  _mapping_block_create(size_t bytes, _mapping** mapping)
{
    // add block onto existing mapping if can, otherwise create
    // new mapping
    // return start of allocation

    _block* end_block = (*mapping)->end_block;
    void* where = (char*)end_block + sizeof(_block) + end_block->sz; 
    size_t block_sz = MY_MALLOC_BLOCK_EXPANSION(bytes);

    if (block_sz > (char*)(*mapping)->end - (char*)where)
    {
        return _mapping_create(bytes, &(*mapping)->next);
    }

    _block_create_unsafe(block_sz, where);

    (*mapping)->end_block = (_block*)where;
    end_block->next = (_block*)where;

    return _block_alloc_unsafe((_block*)where, bytes);
}

// currently get working with single threaded

void*  my_malloc(size_t bytes)
{
    // allocate bytes somewhere on the heap
    // return pointer to allocated space 

    _mapping* mapping = G_global.start_map;
    _block* block = _block_get(bytes, &mapping);
    
    if (block)
    {
        return _block_alloc_unsafe(block, bytes);
    }

    if (!mapping)
    {
        return _mapping_create(bytes, &G_global.start_map);
    }

    return _mapping_block_create(bytes, &mapping);
}

void my_free(void* ptr)
{
}
