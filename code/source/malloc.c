/*  General idea is to organize the heap into conceptual
    linked lists. Where each layer of linked list is an array
    which is iterated through using the size of nodes.

    There are three layers of linked lists
        - Mapping
        - Block
        - Allocations

    Overhead:

        There is overhead at every level of linked list.
        The amount of overhead for Mapping's and Block's
        is neglegable compared to the number of
        allocated bytes.
        Every allocation has MY_MALLOC_ALLOC_META bytes
        of overhead. Usually 8 and 16 bytes for 32 and
        64 bit systems respectively.

    Locking:

        Mutal exclusion is needed at two levels.
            1) Mapping
            2) Block
        
        (1)
        A lock on (1) gives a thread exclusive access
        to that entire linked list level.
            ie no other mapping can be modified
        Locking this level is used for when the linked
        list of mappings needs to be modified. This only
        happens when more memory is needed / removed. An
        expensive operation. Can switch out the thread
        while waiting for this lock.

        (2)
        Locking (2) gives a thread exclusive access
        to only the requested node.
            ie other threads can modify other blocks,
               but not the locked one  
        Locking this level is used for when the linked
        list of blocks in a mapping needs to be
        modified. This can be for a variety of reasons,
        but is a very cheap operation. 

    Allocation:

        An allocation causes a searching all three levels
        of linked lists.
        A first fit strategy is used for allocations.
        Once a space is found in a block, the space
        gets set to taken and the entire block is iterated
        over to update meta data.

    Free:

        A free causes a looping over of the block the
        memory was requested from.

    Constraints:

        - Can allocate at most size_t minus 1 bitwidth
          in one allocation.

    Notes:

        - Returning a void pointer from "my_malloc" then
          having a void pointer as the param to "my_free"
          is undefined behaviour. Since
            some_ptr -> void* -> some_ptr
          is defined. But
            void* -> some_ptr -> void*
          is not defined.

          The second set of conversions is avoided elsewhere.

        - Assume mmap is zero backed. This is very likely to
          be true, but not gaurenteed.
*/

#include <custom_mem/malloc.h>
#include <stdatomic.h>
#include <assert.h>
#include <sys/mman.h> // mmap
#include <stdint.h>   // SIZE_MAX
#include <string.h>   // memset, memcpy

typedef struct MallocGlobal
{
    struct MallocMapping* start_map;

    /*  Whether any mapping is currently
        being modified.
    */
    atomic_char is_free;
}
_global;

/*  Is the top level in linked list chain.
    Mapping's cannot be locked. They consist
    of blocks.

    Every mapping is requested with one
    mmap call.
*/
typedef struct MallocMapping
{
    /* Starting block in this mapping.
    */
    void* start_block;

    /* Ending block in this mapping.
    */
    void* end_block;

    /* Next mapping.
    */
    struct MallocMapping* next;

    /* Start of this mapping.
    */
    void* start;

    /* End of this mapping.
    */
    void* end;
}
_mapping;

/*  Is the second level in linked list chain.
    Blocks can be locked. They consist of
    allocations.

    At any time in the block there is always
    atleast one allocation meta data which
    can be used for an allocation.
    The gaurenteed meta data will be after
    the last used byte in the block. However,
    there may be other free spaces within
    the block.
        ie
        BLOCK_META_DATA         |
        META_DATA (sz_1,used)   |
        ...                     |
            in use memory       |
        ...                     |
        META_DATA (sz_2,free) <- max_free_ptr
        ...             |       |
            free memory | max_free
        ...             |       |
        META_DATA (sz_3,used)   |
        ...                     |
            in use memory       |
        ...                     | sz
        META_DATA (sz_4,free)   |
        ...                     |
            free memory         |
        ...                     |
        END OF BLOCK <- next


        The last meta data before end of block
        will always be there.

    Note: For simplicity, the above block is
          respresented as

        (sz_1,used) -> (sz_2,free) -> (sz_3,used) -> (sz_4,free) ->

        Where sz_N represents the amount of bytes that can be
        allocated in a particular node.
*/
typedef struct MallocBlock
{
    /*  Maximum contiguous number of free contiguous bytes.
    */
    size_t max_free;

    /*  Size in bytes this block takes in its entirety.
    */
    size_t sz;

    /*  Whether being modified currently.
    */
    atomic_char is_free;

    /*  Next block.

        The next block will be directly after the
        current block in memory.
        If no next block, then NULL.

        Keep this here to allow for easy future
        changes.
    */
    void* next;

    /*  Pointer to metadata of where max free space is.
    */
    void* max_free_ptr;
}
_block;

typedef _block* _blk;
typedef _mapping* _map;
typedef struct MallocAdjustables _vars;

#if SIZE_MAX == 0xffffffff
    #define MY_MALLOC_SHIFTER 0x80000000
    #define MY_MALLOC_NUM_BITS 32
#elif SIZE_MAX == 0xffffffffffffffff
    #define MY_MALLOC_SHIFTER 0x8000000000000000
    #define MY_MALLOC_NUM_BITS 64
#else
    static_assert(0, "not 32 or 64 bit system");
#endif

/*  Need "pause" or close to equivalent.

    Ripped from https://github.com/gstrauss/plasma
*/
#if defined(__x86_64__) || defined(__i386__)

  #define MY_MALLOC_PAUSE() \
    __asm__ __volatile__ ("pause")

#elif defined(__ia64__)

  #if (defined(__HP_cc__) || defined(__HP_aCC__))

    #define MY_MALLOC_PAUSE() \
        _Asm_hint(_HINT_PAUSE)

  #else

    #define MY_MALLOC_PAUSE() \
        __asm__ __volatile__ ("hint @pause")

  #endif

#elif defined(__arm__)

  #ifdef __CC_ARM

    #define MY_MALLOC_PAUSE() \
        __yield()

  #else

    #define MY_MALLOC_PAUSE() \
        __asm__ __volatile__ ("yield")

  #endif

#else

    static_assert(0, "Need \"pause\" instruction or similar defined.");

#endif

/*  Need count leading zero's or equivalent.
*/
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)

    #define MY_MALLOC_CLZ(NUM) \
        __builtin_clzl(NUM)

#else

    static_assert(0, "Need clz or equivalent defined.")

#endif

/*  How many bytes should the block take.

    This is needed because for a requested number of
    bytes, there is some extra number of bytes needed
    for meta data.

    {    1    }   {     2      }   {        3         }
    (sz | 1024) + sizeof(_block) + MY_MALLOC_ALLOC_META

    1 - padding room so that every allocation doesn't
        require a new block
    2 - block meta data
    3 - atleast one metadata will be needed for
        atleast one allocation
*/
#define MY_MALLOC_BLOCK_EXPANSION(sz) \
    (((sz) | 1024) + sizeof(_block) + MY_MALLOC_ALLOC_META)

/*  Meta data per allocation.

    meta data is
    size_t - size of this allocation
    +
    void* - start of block meta data which
            allocation is in
*/
#define MY_MALLOC_ALLOC_META \
    (sizeof(size_t) + sizeof(void*))

/*  Get pointer to availability.
*/
#define MY_MALLOC_GET_AVAILABILITY_PTR(VP_META) \
    ((char*)(VP_META) + sizeof(size_t))

// get size of allocation
// should not include meta data
#define MY_MALLOC_GET_SIZE(VP_META) \
    (*(size_t*)(VP_META))

/*  Get whether allocation is free.

    NULL -> free
    else -> not free, start of block
*/
#define MY_MALLOC_GET_AVAILABILITY(VP_META) \
    *(void**)MY_MALLOC_GET_AVAILABILITY_PTR(VP_META)

/* Set size of allocation.
*/
#define MY_MALLOC_SET_SIZE(VP_META, SZ) \
    *(size_t*)(VP_META) = SZ

/*  Set allocation to free.
*/
#define MY_MALLOC_SET_FREE(VP_META) \
    do \
    { \
        void* temp = MY_MALLOC_GET_AVAILABILITY_PTR(VP_META); \
        *(void**)temp = NULL; \
        if (temp); \
    } while (0)

/* Set allocation to inuse.
*/
#define MY_MALLOC_SET_INUSE(VP_META, VP_BLOCK) \
    do \
    { \
        void* temp = MY_MALLOC_GET_AVAILABILITY_PTR(VP_META); \
        *(void**)temp = VP_BLOCK; \
        if (temp); \
    } while (0)

/*  Iterate a meta data pointer forward to the
    next possible meta data.
*/
#define MY_MALLOC_NEXT(VP_META) \
    ((char*)(VP_META) + MY_MALLOC_ALLOC_META + MY_MALLOC_GET_SIZE(VP_META))

#define MY_MALLOC_LOCK_FREE 1

#define MY_MALLOC_LOCK_INSUSE 0

static _global G_global =
{
    .start_map = NULL,
    .is_free   = MY_MALLOC_LOCK_FREE
};

static _vars G_vars =
{
    .more_mem  = 1048576,
    .long_wait =
    {
        0,
        2000
    }
};

static void*  _mem_get(size_t bytes)
{
    // get bytes more memory

    void* res = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (res == (void*)-1)
    {
        return NULL;
    }

    return res;
}

static size_t _mem_more_sz(size_t bytes)
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
    return MY_MALLOC_SHIFTER >> (MY_MALLOC_CLZ(bytes) - 1);
}

static void   _wait_short()
{
    // wait for a relatively shorter period of time

    for (int i = 0; i != 32; ++i)
    {
        MY_MALLOC_PAUSE();
    }
}

static void   _wait_long()
{
    // wait for a relatively longer period of time

    /*  technically speaking, okay if nanosleep fails
        will just be caught later on when depth limit is exceeded
    */

    nanosleep(&G_vars.long_wait, NULL);
}

static void   _block_lock_free(void* block)
{
    // make the block available for modification

    ((_block*)block)->is_free = MY_MALLOC_LOCK_FREE;
    // atomic_store(&((_block*)block)->is_free, MY_MALLOC_LOCK_FREE);
}

static int    _block_has_room(size_t bytes, _block* block)
{
    // whether block has enough room for bytes
    // plus an allocation meta data
    // return 1 if yes, 0 otherwise

    return block->max_free >= bytes;
}

static int    _block_acquire(size_t bytes, void* block)
{
    // acquire sole access to a block so that the
    // block has bytes space available
    // return 1 if successful

    _block* block_ptr = block;
    char expected = MY_MALLOC_LOCK_FREE;

    if (atomic_compare_exchange_strong(&block_ptr->is_free, &expected, MY_MALLOC_LOCK_INSUSE))
    {
        // verify available space after obtained lock
        if (_block_has_room(bytes, block))
        {
            return 1;
        }

        _block_lock_free(block);
    }

    return 0;
}

static void   _block_update_meta(void* block)
{
    // update the block meta data to reflect
    // information in the allocation meta data's

    /*  Algorithm explained.

        Instead of using this representation
            (sz_1,used) -> (sz_2,free) -> (sz_3,used) ->
        Use the following, which only shows whether
        a node is U(sed) or F(ree)
            U -> F -> U ->



        This function is called after at most one
        manipulation has been done to a block. Either
        a free or allocation.The point is to
        correctly update block and allocation meta
        data to a state such that the block is valid.

        A valid block meets all the conditions
            - block meta data is correct
            - at most one free consecutive node

        To update first, loop through all nodes keeping
        track of the largest free node. Updating meta
        data at the end.

        Second condition is harder.
            Consider the following list
                U -> F -> U -> U ->
            which, with a free, can be turned into
                U -> F -> F -> U ->

            Consider as well
                U -> F -> U -> F ->
            which, with a free, can be turned into
                U -> F -> F -> F ->

            Consider
                U -> F -> U ->
            which, with an allocation could be
                U -> U -> U ->

            With any single manipulation, atmost three
            consecutive frees will occur. Need to
            merge [0,3] consecutive frees.
    */

    _block* block_ptr = block;

    size_t i = sizeof(_block);
    void* prev = (char*)block + sizeof(_block);

    while (i < block_ptr->sz && MY_MALLOC_GET_AVAILABILITY(prev))
    {
        i += MY_MALLOC_GET_SIZE(prev) + MY_MALLOC_ALLOC_META;
        prev = MY_MALLOC_NEXT(prev);
    }

    if (i >= block_ptr->sz)
    {
        // didn't find a free node

        block_ptr->max_free = 0;
        block_ptr->max_free_ptr = NULL;

        return;
    }

    void* max = prev, *curr = MY_MALLOC_NEXT(prev);
    i += MY_MALLOC_GET_SIZE(prev) + MY_MALLOC_ALLOC_META;

    /*  If execution reaches here prev will be located at
        the first free allocation's meta data.
        curr (i) will be the possible node after prev.

        A possible state at this point is

        (taken,sz_1) -> (taken,sz_2) -> (free,sz_3) -> (taken/free,sz_4) -> ...
                                        prev           curr (i)

        i = sz_1 + sz_2 + sz_3 + (META * 3) + BLOCK_META
    */

    while (i < block_ptr->sz)
    {
        size_t curr_sz = MY_MALLOC_GET_SIZE(curr);

        // merge previous and curr if both free
        if
        (
            prev
            &&
            !MY_MALLOC_GET_AVAILABILITY(prev)
            &&
            !MY_MALLOC_GET_AVAILABILITY(curr)
        )
        {
            // prev and curr are both free, merge

            size_t old_curr_sz = curr_sz;

            curr_sz += MY_MALLOC_GET_SIZE(prev) + MY_MALLOC_ALLOC_META;

            void* next = MY_MALLOC_NEXT(curr);
            if
            (
                i + MY_MALLOC_GET_SIZE(curr) + MY_MALLOC_ALLOC_META < block_ptr->sz
                &&
                !MY_MALLOC_GET_AVAILABILITY(next)
            )
            {
                // next is also free, merge all three

                curr_sz += MY_MALLOC_GET_SIZE(next) + MY_MALLOC_ALLOC_META;
            }

            MY_MALLOC_SET_SIZE(prev, curr_sz);

            curr = prev;
            prev = NULL;

            curr_sz = old_curr_sz;
        }

        if
        (
            !MY_MALLOC_GET_AVAILABILITY(curr)
            &&
            MY_MALLOC_GET_SIZE(curr) > MY_MALLOC_GET_SIZE(max)
        )
        {
            max = curr;
        }

        i += curr_sz + MY_MALLOC_ALLOC_META;
    }

    block_ptr->max_free = MY_MALLOC_GET_SIZE(max);
    block_ptr->max_free_ptr = max;
}

static void*  _block_alloc_unsafe(size_t bytes, void* block)
{
    // allocate bytes from block and update the largest possible
    // allocation in block
    // return the start of allocation

    // Assume: bytes <= block.max_free - ALLOC_META

    _block* block_ptr = block;

    void* alloc_start = (char*)block_ptr->max_free_ptr + MY_MALLOC_ALLOC_META;

    /*  Always add another allocation meta data.

        For exmaple
        (1500,used) -> (1000,free) -> (2000,used) -> ...
        Can turn into
        (1500,used) -> (984,used) -> (0,free) -> (2000,used) -> ...

        Point is, an allocation meta data will
        always be added to maintain structure. Even
        at the cost of wasted space.
    */
    size_t remaining = block_ptr->max_free - bytes - MY_MALLOC_ALLOC_META;

    MY_MALLOC_SET_INUSE(block_ptr->max_free_ptr, block);
    MY_MALLOC_SET_SIZE(block_ptr->max_free_ptr, bytes);

    // set up meta data for another allocation
    void* after_insert = MY_MALLOC_NEXT(block_ptr->max_free_ptr);
    MY_MALLOC_SET_FREE(after_insert);
    MY_MALLOC_SET_SIZE(after_insert, remaining);

    _block_update_meta(block);

    return alloc_start;
}

static void   _block_create_unsafe(size_t sz, void* where)
{
    // create a block of sz starting the block on where
    // return the total number of bytes taken

    _block* block_ptr = (_block*)where;

    /*  Create the block with a meta data subtracted since
        initialzation requires two meta data's, but all
        other cases require one.
    */
    _block new_block =
    {
        .sz           = sz,
        .is_free      = 1,
        .next         = NULL,
        .max_free_ptr = (char*)where + sizeof(_block),
        .max_free     = sz - sizeof(_block) - MY_MALLOC_ALLOC_META
    };

    *block_ptr = new_block;

    void* initial_alloc = (char*)where + sizeof(_block);
    MY_MALLOC_SET_FREE(initial_alloc);
    MY_MALLOC_SET_SIZE(initial_alloc, block_ptr->max_free);
}

static void*  _block_get(size_t bytes, _mapping** mapping)
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
        if (_block_has_room(bytes, block)) // && _block_lock_acquire
        {
            return block;
        }

        block = ((_block*)block)->next;
    }

    while((*mapping)->next)
    {
        while (block)
        {
            if (_block_has_room(bytes, block)) // && _block_lock_acquire
            {
                return block;
            }

            block = ((_block*)block)->next;
        }

        *mapping = (*mapping)->next;
    }

    return NULL;
}

static int    _mapping_has_room(size_t block_sz, _mapping* mapping)
{
    // whether mapping has enough room for bytes
    // return 1 if yes, 0 otherwise

    _block* end_block = mapping->end_block;
    char* inuse_end = (char*)mapping->end_block + end_block->sz;

    return block_sz > (char*)mapping->end - inuse_end;
}

static _map   _mapping_create_unsafe(size_t sz)
{
    // create mapping capable of holding sz
    // return where the mapping is created

    size_t more_mem = _mem_more_sz(sz + sizeof(_mapping));
    void* start = _mem_get(more_mem);
    if (!start)
    {
        return NULL;
    }

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

static void*  _mapping_create(size_t bytes, _mapping** mapping)
{
    // request a mapping with bytes allocated onto it
    // return the start of allocation

    size_t block_sz = MY_MALLOC_BLOCK_EXPANSION(bytes);
    void* new_mapping = _mapping_create_unsafe(block_sz + sizeof(_mapping));
    if (!new_mapping)
    {
        return NULL;
    }
    *mapping = new_mapping;

    void* where = (char*)new_mapping + sizeof(_mapping);

    _block_create_unsafe(block_sz, where);

    (*mapping)->start_block = where;
    (*mapping)->end_block = where;

    return _block_alloc_unsafe(bytes, where);
}

static void*  _mapping_append_block(size_t block_sz, _mapping* mapping)
{
    // add block to the end of mapping
    // return start of new block

    // Assume: mapping has enough room for block_sz

    _block* end_block = mapping->end_block;
    void* new_block = (char*)mapping->end_block + end_block->sz;
    
    _block_create_unsafe(block_sz, new_block);
    
    mapping->end_block = new_block;
    end_block->next = new_block;

    return new_block;
}

static void*  _advanced_malloc(size_t bytes, char search, void* block, _mapping* mapping)
{
    // get an allocation of bytes beginning by looking
    // from block and mapping
    // only search for block if indicated

    /*  NEED depth limiter on search
    */

    if (search)
    {
        void* block_res = _block_get(bytes, &mapping);

        if (block_res)
        {
            if (_block_acquire(bytes, block_res))
            {
                void* res = _block_alloc_unsafe(bytes, block_res);
                _block_lock_free(block_res);

                return res;
            }
        }
    }

    ++search;

    /*  Failed to find a block. No matter what will have
        to modify atleast one mapping.
    */

    char expected = MY_MALLOC_LOCK_FREE;
    if (atomic_compare_exchange_strong(&G_global.is_free, &expected, MY_MALLOC_LOCK_INSUSE))
    {
        size_t block_sz = MY_MALLOC_BLOCK_EXPANSION(bytes);

        if (!mapping || !_mapping_has_room(block_sz, mapping))
        {
            // mapping is null or does not have enough room
            // for a new block of necessary size

            void* res = _mapping_create(bytes, &mapping);
            atomic_store(&G_global.is_free, MY_MALLOC_LOCK_FREE);

            return res;
        }

        void* new_block = _mapping_append_block(block_sz, mapping);
        atomic_store(&G_global.is_free, MY_MALLOC_LOCK_FREE);

        return _block_alloc_unsafe(bytes, new_block);
    }

    _wait_long();

    return _advanced_malloc(bytes, search, block, mapping);
}

void* my_malloc(size_t bytes)
{
    // allocate bytes somewhere on the heap
    // return pointer to allocated space

    _mapping* mapping = G_global.start_map;
    void* block = _block_get(bytes, &mapping);

    if (block)
    {
        if (_block_acquire(bytes, block))
        {
            void* res = _block_alloc_unsafe(bytes, block);
            _block_lock_free(block);

            return res;
        }
    }

    return _advanced_malloc(bytes, 0, block, mapping);
}

void  my_free(void* ptr)
{
    // set an allocation to be freed

    void* block = MY_MALLOC_GET_AVAILABILITY((char*)ptr - MY_MALLOC_ALLOC_META);

    while (!_block_acquire(0, block))
    {
        _wait_short();
    }

    MY_MALLOC_SET_FREE((char*)ptr - MY_MALLOC_ALLOC_META);
    _block_update_meta(block);
    
    _block_lock_free(block);
}

void* my_calloc(size_t num, size_t bytes)
{
    // allocate zero'd bytes * num bytes if
    // the product does not overflow

    // Note: See "Notes" section at start of file
    //       for zero'ing info

    if (MY_MALLOC_CLZ(num) + MY_MALLOC_CLZ(bytes) < MY_MALLOC_NUM_BITS)
    {
        // would overflow

        return NULL;
    }

    const size_t req_bytes = num * bytes;
    void* res = my_malloc(req_bytes);
    memset(res, 0, req_bytes);

    return res;
}

void* my_realloc(void *ptr, size_t size)
{
    /*  we can always shrink
        check if we can expand the current allocation
            if yes then return expanded
        find block with enough room, ie just normal
        malloc, copy data
    */

    // can reduce the two ifs down

    void* alloc_meta = (char*)ptr - MY_MALLOC_ALLOC_META;
    void* block = MY_MALLOC_GET_AVAILABILITY(alloc_meta);

    while (!_block_acquire(0, block))
    {
        _wait_short();
    }

    size_t old_sz = MY_MALLOC_GET_SIZE(alloc_meta);

    if (size <= old_sz)
    {
        // shrink current allocation

        size_t diff = old_sz - size;
        size_t next_old_sz = MY_MALLOC_GET_SIZE(MY_MALLOC_NEXT(alloc_meta));

        MY_MALLOC_SET_SIZE(alloc_meta, size);

        void* next = MY_MALLOC_NEXT(alloc_meta);
        MY_MALLOC_SET_SIZE(next, next_old_sz + diff);
        MY_MALLOC_SET_INUSE(next, block);

        _block_update_meta(block);

        _block_lock_free(block);

        return ptr;
    }

    void* next = MY_MALLOC_NEXT(alloc_meta);
    if
    (
        !MY_MALLOC_GET_AVAILABILITY(next)
        &&
        size <= old_sz + MY_MALLOC_GET_SIZE(next)
    )
    {
        // expand current allocation

        size_t next_old_sz = MY_MALLOC_GET_SIZE(MY_MALLOC_NEXT(alloc_meta));

        size_t new_next_sz = (next_old_sz + old_sz) -  size;

        MY_MALLOC_SET_SIZE(alloc_meta, size);

        void* next_new = MY_MALLOC_NEXT(alloc_meta);
        MY_MALLOC_SET_SIZE(next_new, new_next_sz);
        MY_MALLOC_SET_INUSE(next_new, block);

        _block_update_meta(block);

        _block_lock_free(block);

        return ptr;
    }

    // new allocation and copy
    void* new_ptr = my_malloc(size);

    // do we need block lock on the new?
    // no since were just copying into an allocation
    // however we need lock on old block so that
    // data doesnt get overwritten
    memcpy(new_ptr, ptr, old_sz);

    return new_ptr;
}
