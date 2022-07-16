#include <custom_mem/malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char const *argv[]) {    

    char* str = malloc_cust(27);
    str = "sup\0";
    printf("%s\n", str);

    return 0;
}