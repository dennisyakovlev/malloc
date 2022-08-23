// request memory in loop

#include <custom_mem/malloc.h>

typedef unsigned long long ull;

#define NUM_FOR_ARR 65536

int main(int argc, char const *argv[])
{
    ull* arr = my_malloc(sizeof(ull) * NUM_FOR_ARR);
    
    for (unsigned long long i = 0; i != NUM_FOR_ARR; ++i)
    {
        arr[i] = i;
    }

    for (unsigned long long i = 0; i != NUM_FOR_ARR; ++i)
    {
        if (arr[i] != i)
        {
            return -1;
        }
    }

    return 0;
}
