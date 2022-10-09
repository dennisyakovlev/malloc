#include <custom_mem/malloc.h>

int main(int argc, char const *argv[])
{
    int* res = my_calloc(1, sizeof(int));
    
    if (*res)
    {
        return -1;
    }

    *res = 8;

    if (*res != 8)
    {
        return -1;
    }

    return 0;
}
