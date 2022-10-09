#include <custom_mem/malloc.h>

int main(int argc, char const *argv[])
{
    int* res = my_malloc(sizeof(int));
    
    *res = 8;

    res = my_realloc(res, 2 * sizeof(int));

    *(res + 1) = -5;

    if (*res != 8 || *(res + 1) != -5)
    {
        return -1;
    }

    return 0;
}
