#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include "time.h"
//-------------------------------
#include <atomic>
#include <vector>
#include <stdint.h>
#include <sys/sysinfo.h>
//-------------------------------
int thread_number = 1;
int cpu_num = 1;
long NumDeq=0,FailDeq=0;
long NumEnq=0,FailEnq=0;
long num = 0;
int running_time=1;
//bool stop = false;
//-------------------------------
const size_t queueSize = 16*1024;
static pthread_barrier_t barrier;
//-------------------------------

struct ringbuffer {
  std::vector<int> data_{};
  alignas(64) std::atomic<size_t> readIdx_{0};
  //alignas(64) size_t writeIdxCached_{0};
  alignas(64) std::atomic<size_t> writeIdx_{0};
  //alignas(64) size_t readIdxCached_{0};

  ringbuffer(size_t capacity) : data_(capacity, -1) {}

  bool push(int val) {
    auto writeIdx = writeIdx_.fetch_add(1, std::memory_order_relaxed);
    writeIdx = writeIdx & (queueSize-1);
    
    if(data_[writeIdx]==-1){
        if(__sync_bool_compare_and_swap(&data_[writeIdx], -1, val)){
            return true;
        }
    }

    return false;
  }

  bool pop(int &val) {
    auto readIdx = readIdx_.fetch_add(1, std::memory_order_relaxed);
    readIdx = readIdx & (queueSize-1);
    
    if(data_[readIdx]!=-1){
        val = data_[readIdx];
        data_[readIdx] = -1;
        return true;
    }

    return false;
  }
};

struct ringbuffer q(queueSize);
//-----------------------------------
void enqueue(int value){

    if(!q.push(value)){
        FailEnq++;
    }
}

int dequeue(){

    int val = -1;
    if(!q.pop(val)){
        FailDeq++;
    }
    return val;
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
        
        enqueue(data+numMyOps);
        
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
    long num = 0;
    elapsed = elapsed_time(0);

    while (numMyOps < 10000000*(thread_number-1)){   
        
        val = dequeue();
        if(val!=-1){
            num++;
        }
        numMyOps++;
    }

    elapsed = elapsed_time(elapsed);
    NumDeq = numMyOps;

    printf("Consumer:The  delay is %ld us\n",elapsed);
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
    
    for(int i=0;i<num_threads;i++){
        pthread_join(threadPool[i],NULL);
    }
    us = elapsed_time(us);
    
    printf("***\ttotal Producer actions are %ld frequency\t***\n",NumEnq);
    printf("***\ttotal Producer failed actions are %ld frequency\t***\n",FailEnq);
    printf("***\ttotal Producer success number is %ld frequency\t***\n",NumEnq-FailEnq);
    printf("***\ttotal Consumer actions are %ld frequency\t***\n",NumDeq);
    printf("***\ttotal Consumer failed actions are %ld frequency\t***\n",FailDeq);
    printf("***\ttotal Consumer success number is %ld frequency\t***\n",NumDeq-FailDeq);
    printf("***\ttotal actions are %ld frequency\t***\n",NumEnq+NumDeq);
    printf("***\ttotal Producer running time is %ld us\t***\n",us);
     
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
    if (argc > 2) {
        running_time = atoi(argv[2]);
    }
    pthread_barrier_init(&barrier, NULL, thread_number);
    testMyqueue(thread_number);
    pthread_barrier_destroy(&barrier);
    return 0;
}
