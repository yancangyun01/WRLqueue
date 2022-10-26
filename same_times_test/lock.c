#include "time.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <stdbool.h>
#include "primitives.h"

#define SIZE 64*1024
static pthread_barrier_t barrier;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t notFull = PTHREAD_COND_INITIALIZER;
pthread_cond_t notEmpty = PTHREAD_COND_INITIALIZER;

int thread_number = 1;
int cpu_num = 1;
long NumDeq=0;
long NumEnq=0;
int head=0;
int tail=0;
int buffer[SIZE];
int running_time=1;
bool stop = false;

void enqueue(int value){
    
    pthread_mutex_lock(&mutex);
    if(head-tail >= SIZE){
        pthread_cond_wait(&notFull, &mutex);
    }

    int head_tmp = head%SIZE;
    buffer[head_tmp] = value;
    head++;
    
    pthread_cond_signal(&notEmpty);
    pthread_mutex_unlock(&mutex);
}

void *dequeue(){
    void *data;
    pthread_mutex_lock(&mutex);
    if(tail >= head){
        pthread_cond_wait(&notEmpty, &mutex);
    }
    
    int tail_tmp = tail%SIZE;
    data = &buffer[tail_tmp];
    tail++;
    pthread_cond_signal(&notFull);
    pthread_mutex_unlock(&mutex);

    return data;
}

void* Producer(void* arg){
    int data = *(int*)arg;

    cpu_set_t set;
    CPU_ZERO(&set);
    int cpu = data%(cpu_num-1);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
    pthread_barrier_wait(&barrier);//等待其他线程准备好，一起开始
    
    long numMyOps = 0;

    while (stop == false){
        enqueue(data);     
        numMyOps++;
    }

    __sync_fetch_and_add(&NumEnq, numMyOps);

    printf("Producer ID %d:number of operations is %ld\n",data,numMyOps);
    return NULL;
}

void* Consumer(void* arg){
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((cpu_num-1), &set);
    sched_setaffinity(0, sizeof(set), &set);
    pthread_barrier_wait(&barrier);//等待其他线程准备好，一起开始
   
    int val = 0;
    long numMyOps = 0;

    while (stop == false){   
        val = *(int*)dequeue();
        numMyOps++;
    }

    NumDeq = numMyOps;
    printf("Consumer:number of data fetched %ld, last dequeue value is %d\n",numMyOps, val);
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
    if (argc > 2) {
        running_time = atoi(argv[2]);
    }
    pthread_barrier_init(&barrier, NULL, thread_number);
    testMyqueue(thread_number);
    pthread_barrier_destroy(&barrier);
    return 0;
}
