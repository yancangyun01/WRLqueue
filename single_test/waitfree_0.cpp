#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include "Wr2Rd_2.h"

TWriter2Reader  twriter2reader_test;
int testnum = 2000;

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

int main(int argc, char* argv[])
{
    if (argc > 1) {
        testnum = atoi(argv[1]);
    }
    void* buffer = malloc(64*1024);
    twriter2reader_test.Init(buffer, 64*1024);
    uint64_t begin;
    uint64_t end;
    uint64_t mean_w=0,max_w=0,mean_r=0,max_r=0;
    int value = 1;

    for (int i=0;i<testnum;i++){
        begin = rdtsc();

        uint32_t myInx;
        uint8_t *p = twriter2reader_test.GetSpace(myInx,sizeof(value));
        if(p)
        {
            *p = value;
            //memcpy(p,value,dataLen);
            twriter2reader_test.Commit(myInx);
        }

        end = rdtsc();
        
        mean_w =mean_w + (end - begin);
        if(max_w<(end - begin)) max_w = (end - begin);
    
    }

    for (int i=0;i<testnum;i++){
        begin = rdtsc();

        twriter2reader_test.Read(true,true);

        end = rdtsc();
        mean_r =mean_r + (end - begin);
        if(max_r<(end - begin)) max_r = (end - begin);
    }

    mean_w = mean_w/testnum;
    mean_r = mean_r/testnum;
    
    printf("Producer:The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean_w,max_w);
    printf("Consumer:The mean delay is %ld ticks, the max dealy is %ld ticks\n",mean_r,max_r);
    return 0;
}
