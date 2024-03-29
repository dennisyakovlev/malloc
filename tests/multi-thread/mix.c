// similar to mix.c in less_basic but
// multi-threaded

#include <custom_mem/malloc.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

atomic_char start_threads;

#define NUM_CALLS 1024

size_t indicies[NUM_CALLS] = { 18,12,64,30,54,62,64,46,29,38,22,51,34,0,15,58,42,30,4,14,29,45,57,30,9,10,34,40,11,64,42,47,0,55,56,56,22,44,52,62,42,49,63,17,35,13,62,10,20,14,46,49,30,8,41,13,24,57,38,52,61,60,16,19,22,57,53,6,47,54,43,55,6,4,38,13,19,49,17,16,18,37,50,15,8,15,32,53,1,14,33,10,41,44,18,16,45,25,43,4,39,36,58,18,60,33,6,6,37,20,7,6,25,54,0,23,32,34,13,37,19,22,56,54,9,8,61,31,28,24,21,16,1,57,25,31,45,48,17,26,45,40,31,40,48,61,63,55,45,2,35,22,46,41,26,42,14,11,64,20,54,9,32,57,64,52,61,30,43,62,1,14,50,5,55,50,6,14,62,37,22,8,38,59,11,49,18,16,55,36,36,53,4,17,62,60,41,58,14,54,32,29,50,47,25,18,19,15,3,12,24,35,39,35,62,61,57,64,51,56,25,33,44,1,1,41,13,59,63,22,43,2,5,14,59,56,39,32,1,46,23,57,51,55,3,43,27,19,44,51,62,14,39,39,9,22,57,26,12,42,17,8,17,40,33,44,16,31,61,16,64,44,3,16,28,43,56,20,55,18,21,22,35,6,62,4,14,61,11,6,1,3,35,2,26,14,24,43,64,0,57,56,16,17,29,15,22,13,46,15,14,52,14,29,54,60,16,32,38,31,5,42,62,12,21,12,23,19,63,60,58,60,51,22,36,0,39,48,29,50,35,23,52,42,7,6,33,53,9,15,2,33,38,46,58,7,43,5,18,47,11,55,63,58,46,13,18,15,26,0,5,27,17,6,49,4,41,44,39,38,16,15,13,54,18,57,19,24,11,15,34,30,32,55,51,4,20,60,43,17,2,62,42,64,27,10,5,32,34,45,43,57,10,21,23,1,63,35,58,12,20,51,29,59,57,40,59,63,10,4,18,44,1,26,49,44,18,39,45,62,18,3,28,29,23,5,21,33,0,1,27,50,6,61,58,44,40,29,12,19,39,32,26,16,57,8,64,47,15,19,35,23,26,2,48,37,27,60,54,60,35,22,54,34,36,4,45,49,34,20,21,43,48,22,10,27,23,15,18,11,56,58,47,5,47,23,14,29,59,40,40,59,4,37,17,28,60,3,32,35,42,6,3,37,58,28,7,31,8,51,38,61,45,45,56,29,51,6,27,7,40,34,50,20,29,29,20,20,18,30,28,44,42,3,10,58,35,62,8,10,26,34,17,15,20,44,27,44,43,15,8,49,54,36,37,10,60,62,10,8,51,26,8,29,3,21,9,3,0,21,24,56,21,42,13,59,20,12,61,51,36,48,63,2,12,42,40,12,17,7,24,4,28,3,62,58,46,16,14,61,1,32,54,30,7,60,29,8,12,53,60,12,21,19,12,44,49,59,36,56,23,21,5,48,64,33,46,12,46,63,19,23,14,26,35,41,32,32,20,15,49,7,13,22,41,54,15,48,7,0,46,48,12,22,20,61,31,52,2,42,48,41,11,3,44,42,38,25,19,44,4,20,43,33,40,42,36,28,52,14,55,9,12,26,62,62,58,40,28,4,23,39,18,26,18,59,45,43,59,22,43,24,23,41,23,55,6,62,36,9,44,23,61,40,62,39,0,37,57,36,0,10,22,0,19,60,43,38,28,30,36,44,15,24,51,52,4,60,47,18,26,13,6,42,42,43,51,32,54,14,61,42,16,34,62,60,23,37,3,40,23,21,31,29,5,29,45,37,12,61,37,23,35,0,41,41,54,38,51,54,22,20,5,30,0,17,47,40,39,62,63,44,2,6,31,48,25,21,27,36,55,35,3,22,55,44,33,18,36,64,31,23,19,40,35,38,63,14,55,5,53,19,17,35,11,35,12,23,11,56,56,33,62,0,2,51,4,9,20,45,36,32,30,41,19,30,9,5,0,13,35,0,35,24,5,26,55,10,6,61,0,20,54,21,61,50,59,51,11,52,5,36,39,41,44,48,45,13,36,14,36,34,48,23,3,35,18,42,44,17,28,24,33,59,54,42,58,27,21,50,29,49,38,50,40,50,4,62,57,42,21,63,15,0,14,23,43,8,10,60,18,26,63,10,47,2,6,55,59,6,57,6,57,35,33,62,10,12,52,61,48,44,16,37,34,24,5,53,42,12,64,1,6,36,46,35,41,35,1,44,62,18,64,59,31,31,34,9,57,54,22,21,56,28,62,43,31,26,1,34,59,38,25,33,47,20,56,29,59,57,12,35,9,43,35,30,53,61,25,14,60,19,15,45 };

void* start(void* thread_num)
{
    int* arr[65] = { 0 };
    size_t thread = *(size_t*)thread_num;

    while (!start_threads)
    {
        for (int i = 0; i != NUM_CALLS; ++i)
        {
            const int index = indicies[i];
            if (arr[index])
            {
                my_free(arr[index]);
                arr[index] = NULL;
            }

            size_t bytes = 0;
            for (int j = 0; j != index && i + j < NUM_CALLS; ++j)
            {
                bytes += indicies[i + j];
            }
            bytes = (bytes * thread) % 262144;

            arr[index] = my_malloc(bytes);
        }
    }

    return NULL;
}

int main(int argc,char const *argv[])
{
    const long num_create = sysconf(_SC_NPROCESSORS_ONLN);
    pthread_t threads[num_create];
    size_t    nums[num_create];

    for (size_t i = 0; i != num_create; ++i)
    {
        nums[i] = i;

        if (pthread_create(&threads[i], NULL, start, &nums[i]))
        {
            return 1;
        }
    }

    atomic_store(&start_threads,1);

    for (size_t i = 0; i != num_create; ++i)
    {
        if (pthread_join(threads[i], NULL))
        {
            return 1;
        }
    }   

    return 0;
}
