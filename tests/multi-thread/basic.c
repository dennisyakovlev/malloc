// test that malloc is multi thread safe with
// very basic test.
// surely if there is a major issue, this
// simple test will detect it and hint that
// something is wrong soley with a request
// for memory

#include <custom_mem/malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

atomic_char start_threads;

void* start(void* _)
{
    while (!start_threads)
    {
        for (int i = 0; i != 1024; ++i)
        {
            my_malloc(64);
        }
    }

    return NULL;
}

int main(int argc, char const *argv[])
{
    const long num_create = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[num_create];

    for (size_t i = 0; i != num_create; ++i)
    {
        if (pthread_create(&threads[i], NULL, start, NULL))
        {
            return 1;
        }
    }

    atomic_store(&start_threads, 1);

    for (size_t i = 0; i != num_create; ++i)
    {
        if (pthread_join(threads[i], NULL))
        {
            return 1;
        }
    }   

    return 0;
}
