#include "time.h"
#include "Wr2Rdup.h"
#include "SyncAtomic.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <stdbool.h>

TBuffInfo Buffs[2]; //The two buffers, one for reader, one for writers
static uint32_t  nItem;    //length of buff for each Buffs
static record_t * q;
//-------------------------------------------------------
static pthread_barrier_t barrier;
int thread_number = 1;
int cpu_num = 1;
long NumDeq=0;
long NumEnq=0;
long num = 0;
// int running_time=1;
// bool stop = false;
//-------------------------------------------------------
//Make sure no writer is using the object before call this func!
void Init(void *buff, uint32_t totalItem)
{
    nItem = (totalItem>>1);
    q = align_malloc(PAGE_SIZE, sizeof(record_t));    
    Buffs[0].buff = (uint8_t*)buff;
    Buffs[1].buff = Buffs[0].buff+nItem;
}

uint8_t *GetSpace(uint32_t &myInx,int32_t num)
{
	//myInx = FAA(&q->dataInx,0); //get the writer buffer
	myInx = q->dataInx;

	FAA(&Buffs[myInx].nWriter,1); //register writer

    uint32_t inx = FAA(&Buffs[myInx].writePos,(uint32_t)num); //allocate slots

	//no enough slots left, free back and return NULL
	//It is OK to free back if allocation fail. Because if one writer fail, following writers will also fail.
    if(inx>=nItem)
    {
        FAA(&Buffs[myInx].writePos,(uint32_t)-num);
        FAA(&Buffs[myInx].nWriter,-1); //dec writer.
        FAA(&q->lostCnt,(uint32_t)num);
        return NULL;
    }

    return Buffs[myInx].buff+inx;
}

void Commit(uint32_t &myInx)
{
    FAA(&Buffs[myInx].nWriter,-1);
}

void AddMsg(int data, int dataLen)
{
    uint32_t myInx;
    uint8_t *p = GetSpace(myInx,dataLen);

    if(p)
    {
        *p = data;
        Commit(myInx);
    }
}

void WaitDataReady(int inx,bool hotWait)
{
	//Both hot spin to wait or delay to wait may NOT work on some system!
	//while(FAA(&Buffs[inx].nWriter,0)>0)
	while(Buffs[inx].nWriter>0)
	{
		 if(hotWait==false)
		 	SysSleep(1);
	}
}

//We assume one reader!!
//Return value is the index to Buffs for read
int Read(bool waitReady,bool hotWait)
{
    uint32_t inx    = q->dataInx;
	uint32_t newInx = inx^1;
	Buffs[newInx].writePos = 0; //reset buffer;
	SWAP(&q->dataInx,newInx); //we use atomic op to make sure cache flush

	if(waitReady)
	{
		WaitDataReady(inx,hotWait);
	}

    return inx;
}

void enqueue(int value){

    AddMsg(value,sizeof(value));
}

void *dequeue(){

    void *length;
    int index = Read(true,true);
    length = (int*)&Buffs[index].writePos;
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

    printf("Consumer:number of data fetched %ld, the  delay is %ld us\n",num,elapsed);
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
    if(NumEnq>=num){
        printf("***\tThe write data failed number is %u\t***\n",q->lostCnt/4);
    }
}

int main(int argc, char* argv[])
{
    void* buffer = malloc(128*1024);
    Init(buffer, 128*1024);
    
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
    // if (argc > 2) {
    //     running_time = atoi(argv[2]);
    // }
    pthread_barrier_init(&barrier, NULL, thread_number);
    testMyqueue(thread_number);
    pthread_barrier_destroy(&barrier);
    return 0;
}
