#include <custom_mem/malloc.h>
#include <stdio.h>

int main(int argc, char const *argv[]) {

    char* str = malloc_cust(4);
    *str = 'a';
    *(str + 1) = 'b';
    *(str + 2) = 'c';
    *(str + 3) = '\0';
    printf("%s\n", str);

    char* str2 = malloc_cust(4);
    *str2 = 'd';
    *(str2 + 1) = 'e';
    *(str2 + 2) = 'f';
    *(str2 + 3) = '\0';
    printf("%s\n", str2);

    return 0;
}