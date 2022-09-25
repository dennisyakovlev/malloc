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
*/

#include <custom_mem/malloc.h>
#include <stdatomic.h>
#include <assert.h>
#include <sys/mman.h> // mmap
#include <stdint.h>   // SIZE_MAX

typedef struct MallocGlobal
{
    struct MallocMapping* start_map;
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
    The gaurenteed meta data will be at the
    end of the block. However, there may
    be other free spaces within the block.
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
    volatile atomic_char is_free;

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

static _global G_global =
{
    .start_map = NULL
};

static _vars G_vars =
{
    .more_mem = 1048576,
    .cache_sz = 64
};

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
    // plus an allocation meta data
    // return 1 is yes, 0 otherwise

    const size_t free = block->max_free;
    if (free <= MY_MALLOC_ALLOC_META)
    {
        return 0;
    }

    return block->max_free - MY_MALLOC_ALLOC_META >= bytes;
}

void   _block_update_meta(void* block)
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

            With any single free, atmost three
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

void*  _block_alloc_unsafe(size_t bytes, void* block)
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

void   _block_create_unsafe(size_t sz, void* where)
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
    // MY_MALLOC_SET_SIZE(initial_alloc, block_ptr->sz);
    // used to be this... ???
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
    void* where = (char*)(*mapping)->end_block + end_block->sz;
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

void   my_free(void* ptr)
{
    // set an allocation to be freed

    void* block = MY_MALLOC_GET_AVAILABILITY((char*)ptr - MY_MALLOC_ALLOC_META);
    MY_MALLOC_SET_FREE((char*)ptr - MY_MALLOC_ALLOC_META);
    _block_update_meta(block);
}
