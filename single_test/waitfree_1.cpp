#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include "Wr2Rd_2.h"

TWriter2Reader  twriter2reader_test;
int testnum = 800;

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
    void* buffer = malloc(2*4*testnum);
    twriter2reader_test.Init(buffer, 2*4*testnum);
    uint64_t begin;
    uint64_t end;
    uint64_t middle;
    uint64_t mean1=0,max1=0,mean2=0,max2=0;
    int value = 1;

    for (int i=0;i<testnum;i++){
        begin = rdtsc();

        uint32_t myInx;
        uint8_t *p = twriter2reader_test.GetSpace(myInx,sizeof(value));

        middle = rdtsc();

        if(p)
        {
            memcpy(p,&value,sizeof(value));
            twriter2reader_test.Commit(myInx);
        }

        end = rdtsc();
        mean1 = mean1 + (middle - begin);
        if(max1<(middle - begin)) max1 = (middle - begin);
        mean2 = mean2 + (end - middle);
        if(max2<(end - middle)) max2 = (end - middle);    
    }
    //成功写数据延迟
    mean1 = mean1/testnum;
    mean2 = mean2/testnum;
    printf("When write data is successful:\n\t creat a space mean ticks is %ld, max ticks is %ld\n\t store the data mean ticks is %ld, max ticks is %ld\n",mean1,max1,mean2,max2);

    //失败写数据延迟
    mean1=0,max1=0,mean2=0,max2=0;
    for (int i=testnum;i<2*testnum;i++){
        begin = rdtsc();

        uint32_t myInx;
        uint8_t *p = twriter2reader_test.GetSpace(myInx,sizeof(value));

        middle = rdtsc();

        if(p)
        {
            memcpy(p,&value,sizeof(value));
            twriter2reader_test.Commit(myInx);
        }

        end = rdtsc();
        mean1 = mean1 + (middle - begin);
        if(max1<(middle - begin)) max1 = (middle - begin);
        mean2 = mean2 + (end - middle);
        if(max2<(end - middle)) max2 = (end - middle);    
    }
    mean1 = mean1/testnum;
    mean2 = mean2/testnum;
    printf("When write data is faild:\n\t creat a space mean ticks is %ld, max ticks is %ld\n\t store the data mean ticks is %ld, max ticks is %ld\n",mean1,max1,mean2,max2);

    //读数据延迟
    mean1=0,max1=0;
    for (int i=0;i<testnum;i++){

        begin = rdtsc();

        twriter2reader_test.Read(true,true);

        end = rdtsc();
        
        mean1 = mean1 + (end - begin);
        if(max1<(end - begin)) max1 = (end - begin);
    }
    
    mean1 = mean1/testnum;
    printf("\t read data mean ticks is %ld,max ticks is %ld \n",mean1,max1);
    return 0;
}
