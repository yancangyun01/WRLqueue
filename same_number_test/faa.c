#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sched.h>
#include <stdbool.h>

#include "primitives.h"
#include "align.h"
#include "time.h"

// uint32_t P=0;
// uint32_t C=0;
int thread_number = 1;
int cpu_num = 1;
long NumDeq=0;
long NumEnq=0;
bool run = false;
//----------------------------------------------------
typedef struct {
  volatile long P DOUBLE_CACHE_ALIGNED;
  volatile long C DOUBLE_CACHE_ALIGNED;
} queue_t DOUBLE_CACHE_ALIGNED;

typedef int handle_t;

static queue_t * q;
static handle_t ** hds;

void queue_init(queue_t * q, int nprocs) {}
void queue_register(queue_t * q, handle_t * hd, int id)
{
  *hd = id + 1;
}

void enqueue(queue_t * q, handle_t * th, void * val)
{
  FAA(&q->P, 1);
}

void * dequeue(queue_t * q, handle_t * th)
{
  FAA(&q->C, 1);
  return (void *) (long) *th;
}

void queue_free(queue_t * q, handle_t * h) {}

void thread_init(int id) {
  hds[id] = align_malloc(PAGE_SIZE, sizeof(handle_t));
  queue_register(q, hds[id], id);
}
//----------------------------------------------------

void* Producer(void* arg){
    int data = *(int*)arg;
    thread_init(data);
    long numMyOps = 0;
    void * val = (void *) (intptr_t) (data);
    handle_t * th = hds[data];
    //---------------------------------
    //内核绑定
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(data, &set);
    pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
    //---------------------------------
    //设置优先级
    struct sched_param sp;
    bzero((void*)&sp, sizeof(sp));
    int policy = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = policy;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    //---------------------------------
    uint64_t begin=0;
    uint64_t end=0;
    uint64_t mean=0,max=0;
    while (run == false){
        sched_yield();
    }

    while (numMyOps < 3000){
        begin = rdtsc();

        enqueue(q, th, val);

        end = rdtsc();
        mean = mean + (end-begin);
        if(max<(end - begin)) max = (end - begin);

        numMyOps++;
    }
    mean = mean/numMyOps;
    printf("\tProducer ID %d: mean ticks is %ld ,max ticks is %ld \n",data,mean,max);

    __sync_fetch_and_add(&NumEnq, numMyOps);
    printf("Producer ID %d:number of operations is %ld\n",data,numMyOps);
    queue_free(q, hds[data]);
    return NULL;
}

void* Consumer(void* arg){
    thread_init(0);
    long numMyOps = 0;
    void * val=(void *) (intptr_t) (0);
    int data = 0;
    handle_t * th = hds[0];
    //---------------------------------
    //内核绑定
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(data, &set);
    pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
    //---------------------------------
    //设置优先级
    struct sched_param sp;
    bzero((void*)&sp, sizeof(sp));
    int policy = sched_get_priority_max(SCHED_FIFO);
    sp.sched_priority = policy;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
    //---------------------------------
    uint64_t begin=0;
    uint64_t end=0;
    uint64_t mean=0,max=0;
    while (run == false){
        sched_yield();
    }

    while (numMyOps < 3000){   
        begin = rdtsc();

        val = dequeue(q, th);
        data = (int) (intptr_t)val;

        end = rdtsc();
        mean = mean + (end-begin);
        if(max<(end - begin)) max = (end - begin);

        numMyOps++;
    }
    mean = mean/numMyOps;
    printf("\tConsumer: mean ticks is %ld ,max ticks is %ld \n",mean,max);

    NumDeq = numMyOps;

    printf("Consumer:number of data fetched %ld, last dequeue value is %d\n",numMyOps, data);
    queue_free(q, hds[0]);
    return NULL;
}

void testMyqueue(int num_threads){
    pthread_t threadPool[num_threads];
    int data[num_threads+1];

    //初始化队列
    q = align_malloc(PAGE_SIZE, sizeof(queue_t));
    queue_init(q, num_threads);
    hds = align_malloc(PAGE_SIZE, sizeof(handle_t * [num_threads]));

    if(pthread_create(&threadPool[0], NULL, Consumer, NULL) != 0){
        printf("ERROR, fail to create consumer\n");
        exit(1);
    }
    for(int i=1;i<num_threads;i++){
        data[i] = i;
        if(pthread_create(&threadPool[i], NULL, Producer, &data[i]) != 0){
        printf("ERROR, fail to create consumer\n");
        exit(1);
        }
    }
    run = true;

    for(int i=0;i<num_threads;i++){
        pthread_join(threadPool[i],NULL);
    }

    printf("***\ttotal Producer actions are %ld frequency\t***\n",NumEnq);
    printf("***\ttotal Consumer actions are %ld frequency\t***\n",NumDeq);
    printf("***\ttotal actions are %ld frequency\t***\n",NumEnq+NumDeq);
    if(NumEnq>=NumDeq){
        printf("***\tThe write data failed number is %ld\t***\n",NumEnq-NumDeq);
    }
}

int main(int argc, char* argv[])
{
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
    
    testMyqueue(thread_number);

    return 0;
}
