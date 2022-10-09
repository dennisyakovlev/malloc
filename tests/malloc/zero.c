#include <custom_mem/malloc.h>

#define NUM_ITERS 256

int main(int argc, char const *argv[])
{
    void* res[NUM_ITERS] = { NULL };
    for (int i = 0; i != NUM_ITERS; ++i)
    {
        res[i] = my_malloc(0);
    }

    /*  one is null, all must be null

        Think of

            [0,0,0,0,0,45,0,0,0,...]
            [3,4,6,2,0,5,...]
            
            if first is NULL:
                next should always be NULL
            if first is not NULL:
                next should never be NULL
    */
    for (int i = 0; i != NUM_ITERS; ++i)
    {
        if (res[0] && !res[i])
        {
            return -1;
        }

        if (!res[0] && res[i])
        {
            return -1;
        }
    }

    if (!res[0])
    {
        return 0;
    }

    // not null, every return should be a
    // unique pointer
    for (int i = 0; i != NUM_ITERS; ++i)
    {
        for (int j = i + 1; j != NUM_ITERS; ++j)
        {
            if (res[i] == res[j])
            {
                return -1;
            }
        }
    }

    return 0;
}
