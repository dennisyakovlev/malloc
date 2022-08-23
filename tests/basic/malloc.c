#include <custom_mem/malloc.h>

int main(int argc, char const *argv[])
{
    int* res = my_malloc(sizeof(int));
    
    *res = 8;

    if (*res != 8)
    {
        return -1;
    }

    return 0;
}
