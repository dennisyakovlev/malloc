#include <custom_mem/malloc.h>
#include <stddef.h>

#include <stdio.h>

int main(int argc, char const *argv[])
{
    size_t max = 0;
    max -= 1;
    void* res = my_calloc(max, max);
    
    if (res)
    {
        return -1;
    }

    // check minimum overflow value
    size_t temp = max, num_digits = 0;
    while (temp)
    {
        temp >>= 1;
        ++num_digits;
    }
    size_t one = 1, half = one << (num_digits / 2);

    res = my_calloc(half, half);

    if (res)
    {
        return -1;
    }

    return 0;
}
