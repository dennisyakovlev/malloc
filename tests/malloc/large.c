// request more than 1 gigabyte at once

#include <custom_mem/malloc.h>

#define NUM_INTS (1307420601 / sizeof(int))

int main(int argc, char const *argv[])
{
    int* arr = my_malloc(sizeof(int) * NUM_INTS);
    
    for (int i = NUM_INTS - 1; i != -1; --i)
    {
        arr[i] = (-1 * (i % 2)) * i;
    }

    for (int i = 0; i != NUM_INTS; ++i)
    {
        if (arr[i] != (-1 * (i % 2)) * i)
        {
            return -1;
        }
    }

    return 0;
}
