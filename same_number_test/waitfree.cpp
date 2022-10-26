#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include "Wr2Rd.h"

static pthread_barrier_t barrier;
TWriter2Reader  twriter2reader_test;
int thread_number = 1;
int cpu_num = 1;
long NumDeq=0;
long NumEnq=0;
long num = 0;

static size_t elapsed_time(size_t us)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000000 + t.tv_usec - us;
}

void enqueue(int value){

    twriter2reader_test.AddMsg(&value,sizeof(value));
}

void *dequeue(){

    void *length;
    int index = twriter2reader_test.Read(true,true);
    length = (int*)&twriter2reader_test.Buffs[index].writePos;
    return length;
}

void* Producer(void* arg){
    int data = *(int*)arg;

    cpu_set_t set;
    CPU_ZERO(&set);
    int cpu = data%(cpu_num-1);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
    pthread_barrier_wait(&barrier);//等待其他线程准备好，一起开始
    
    long elapsed;
    long numMyOps = 0;
    elapsed = elapsed_time(0);

    while (numMyOps < 10000000){
        enqueue(data);     
        numMyOps++;
    }

    elapsed = elapsed_time(elapsed);
    __sync_fetch_and_add(&NumEnq, numMyOps);

    printf("Producer ID %d:The  delay is %ld us\n",data,elapsed);
    return NULL;
}

void* Consumer(void* arg){
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((cpu_num-1), &set);
    sched_setaffinity(0, sizeof(set), &set);
    pthread_barrier_wait(&barrier);//等待其他线程准备好，一起开始
   
    long elapsed;
    int val = 0;
    long numMyOps = 0;
    elapsed = elapsed_time(0);

    while (numMyOps < 10000000*(thread_number-1)){   
        val = *(int*)dequeue();
        num = num + val/4;
        numMyOps++;
    }

    elapsed = elapsed_time(elapsed);
    NumDeq = numMyOps;

    printf("Consumer:The  delay is %ld us\n",elapsed);
    printf("Consumer:number of data fetched %ld \n",num);
    return NULL;
}

void testMyqueue(int num_threads){
    pthread_t threadPool[num_threads];
    int data[num_threads+1];

    data[0] = 0;

    if (num_threads>1){
        if(pthread_create(&threadPool[0], NULL, Consumer, NULL) != 0){
        printf("ERROR, fail to create consumer\n");
        exit(1);
        }
    }
    else{
        if(pthread_create(&threadPool[0], NULL, Producer, &data[0]) != 0){
        printf("ERROR, fail to create producer\n");
        exit(1);
        }
    }
    for(int i=1;i<num_threads;i++){
        data[i] = i;
        if(pthread_create(&threadPool[i], NULL, Producer, &data[i]) != 0){
        printf("ERROR, fail to create consumer\n");
        exit(1);
        }
    }

    long us = elapsed_time(0);
    for(int i=0;i<num_threads;i++){
        pthread_join(threadPool[i],NULL);
    }
    us = elapsed_time(us);

    printf("***\ttotal Producer actions are %ld frequency\t***\n",NumEnq);
    printf("***\ttotal Consumer actions are %ld frequency\t***\n",NumDeq);
    printf("***\ttotal actions are %ld frequency\t***\n",NumEnq+NumDeq);
    printf("***\ttotal Producer running time is %ld us\t***\n",us);
    if(NumEnq>=NumDeq){
        printf("***\tThe write data failed number is %ld\t***\n",NumEnq-num);
    }
}

int main(int argc, char* argv[])
{
    void* buffer = malloc(40960);
    twriter2reader_test.Init(buffer, 40960);
    
    if (argc < 1)
	{
		printf(" **********need to specify the amount of threads and running time************* \n");
		return 0;
	}
    printf("  Benchmark: %s\n", argv[0]);
    
    if (argc > 1) {
        thread_number = atoi(argv[1]);
    }
    cpu_num = get_nprocs();
    printf("  Number of processors: %d\n", thread_number);
    pthread_barrier_init(&barrier, NULL, thread_number);
    testMyqueue(thread_number);
    pthread_barrier_destroy(&barrier);
    return 0;
}
