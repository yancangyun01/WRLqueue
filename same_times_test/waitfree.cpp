#include "time.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <stdbool.h>
#include "Wr2Rd_2.h"

static pthread_barrier_t barrier;
TWriter2Reader  twriter2reader_test;
int thread_number = 1;
int cpu_num = 1;
long NumDeq=0;
long NumEnq=0;
long num = 0;
int running_time=1;
bool stop = false;

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
    // uint64_t begin;
    // uint64_t end;
    // uint64_t test=0;
    cpu_set_t set;
    CPU_ZERO(&set);
    int cpu = data%(cpu_num-1);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
    pthread_barrier_wait(&barrier);//等待其他线程准备好，一起开始
    
    long numMyOps = 0;

    while (stop == false){
        // begin = rdtsc();
        enqueue(data);
        // end = rdtsc();
        // test = test+(end-begin);
        numMyOps++;
    }
    // test = test/numMyOps;
    // printf("\tProducer ID %d: ticks test output is %ld\n",data,test);
    __sync_fetch_and_add(&NumEnq, numMyOps);

    printf("Producer ID %d:number of operations is %ld\n",data,numMyOps);
    return NULL;
}

void* Consumer(void* arg){
    // uint64_t begin;
    // uint64_t end;
    // uint64_t test=0;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((cpu_num-1), &set);
    sched_setaffinity(0, sizeof(set), &set);
    pthread_barrier_wait(&barrier);//等待其他线程准备好，一起开始
   
    int val = 0;
    long numMyOps = 0;

    while (stop == false){   
        // begin = rdtsc();
        val = *(int*)dequeue();
        num = num + val/4;
        // end = rdtsc();
        // test = test+(end-begin);
        numMyOps++;
    }
    // test = test/numMyOps;
    // printf("\tConsumer: ticks test output is %ld\n",test);
    NumDeq = numMyOps;

    printf("Consumer:number of data fetched %ld, last dequeue value is %d\n",num,val);
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
        printf("ERROR, fail to create producer\n");
        exit(1);
        }
    }

    long us = elapsed_time(0);
    sleep(running_time);
    stop = true;
    for(int i=0;i<num_threads;i++){
        pthread_join(threadPool[i],NULL);
    }
    us = elapsed_time(us);
    
    printf("***\ttotal Producer actions are %ld frequency\t***\n",NumEnq);
    printf("***\ttotal Consumer actions are %ld frequency\t***\n",NumDeq);
    printf("***\ttotal actions are %ld frequency\t***\n",NumEnq+NumDeq);
    printf("***\ttotal Producer running time is %ld us\t***\n",us);
    if(NumEnq>=num){
        printf("***\tThe write data failed number is %u\t***\n",twriter2reader_test.lostCnt/4);
    }
}

int main(int argc, char* argv[])
{
    void* buffer = malloc(128*1024);
    twriter2reader_test.Init(buffer, 128*1024);
    memset(buffer,0,sizeof(128*1024));
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
    if (argc > 2) {
        running_time = atoi(argv[2]);
    }
    pthread_barrier_init(&barrier, NULL, thread_number);
    testMyqueue(thread_number);
    pthread_barrier_destroy(&barrier);
    return 0;
}
