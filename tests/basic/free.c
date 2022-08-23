#include <custom_mem/malloc.h>

int main(int argc, char const *argv[])
{
    char* res = my_malloc(sizeof(char));

    my_free(res);

    return 0;
}
