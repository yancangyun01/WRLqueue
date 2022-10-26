#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <stdbool.h>
#include "primitives.h"

#define SIZE 1000
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int head=0;
int tail=0;
int buffer[SIZE];
int testnum = 2000;

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

int main(int argc, char* argv[])
{
    uint64_t begin;
    uint64_t end;
    uint64_t mean=0,max=0;
    if (argc > 1) {
        testnum = atoi(argv[1]);
    }
    for (int i=0;i<testnum;i++){
        begin = rdtsc();
        
        pthread_mutex_lock(&mutex);
        int head_tmp = head%SIZE;
        buffer[head_tmp] = 1;
        head++;
        pthread_mutex_unlock(&mutex);

        end = rdtsc();
        mean =mean + (end - begin);
        if(max<(end - begin)) max = (end - begin);
        
    }
    
    mean = mean/testnum;

    printf("The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean,max);
    return 0;
}
