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
#include "align.h"

typedef struct {
  volatile long P DOUBLE_CACHE_ALIGNED;
  volatile long C DOUBLE_CACHE_ALIGNED;
} queue_t DOUBLE_CACHE_ALIGNED;

static queue_t * q;
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
    if (argc > 1) {
        testnum = atoi(argv[1]);
    }
    q = align_malloc(PAGE_SIZE, sizeof(queue_t));

    uint64_t mean_w=0,max_w=0,mean_r=0,max_r=0;

    for (int i=0;i<testnum;i++){
        begin = rdtsc();
        FAA(&q->P, 1);
        end = rdtsc();
        mean_w =mean_w + (end - begin);
        if(max_w<(end - begin)) max_w = (end - begin);
    }

    for (int i=0;i<testnum;i++){
        begin = rdtsc();
        
        FAA(&q->C, 1);
        
        end = rdtsc();
        mean_r =mean_r + (end - begin);
        if(max_r<(end - begin)) max_r = (end - begin);
    }

    uint64_t err = 0;
    for (int k=0;k<testnum;k++){
        begin = rdtsc();
        end = rdtsc();
        err = err+(end - begin);  
    }
    err = err/testnum;
    printf("error is %ld ticks\n",err);
    
    mean_w = mean_w/testnum;
    mean_r = mean_r/testnum;

    printf("Producer:The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean_w,max_w);
    printf("Consumer:The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean_w,max_w);

}
