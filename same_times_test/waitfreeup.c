// #include "time.h"
// #include "Wr2Rdup.h"
// #include <pthread.h>
// #include <unistd.h>
// #include <sys/sysinfo.h>
// #include <stdbool.h>
// #include <stdatomic.h>
// #include <sched.h>

// TBuffInfo Buffs[2]; //The two buffers, one for reader, one for writers
// static uint32_t  nItem;    //length of buff for each Buffs
// static record_t * q;
// //-------------------------------------------------------
// bool run = false;
// int running_time=1;
// bool stop = false;
// int thread_number = 1;
// int cpu_num = 1;
// long NumDeq=0;
// long NumEnq=0;
// long num = 0;
//-------------------------------------------------------
//Make sure no writer is using the object before call this func!
void Init(void *buff, uint32_t totalItem)
{
    nItem = (totalItem>>1);
    q = align_malloc(PAGE_SIZE, sizeof(record_t));
    memset(Buffs,0,sizeof(Buffs));
    memset(q,0,sizeof(record_t));    
    Buffs[0].buff = (uint8_t*)buff;
    Buffs[1].buff = Buffs[0].buff+nItem;
}

void Commit(uint32_t *myInx)
{
    while(1){
    	uint64_t olddata = atomic_load(&q->CitWriter);
        uint64_t newdata = olddata-2;
    	if (*myInx != (olddata & 0x01)){  
        	FAA(&Buffs[*myInx^1].nWriter,1);
        	return ;
    	}
    	else if (CAS(&q->CitWriter,&olddata,newdata)){
        	return ; 
    	}
    }
}

uint8_t *GetSpace(uint32_t *myInx,int32_t num)
{
	*myInx = FAA(&q->CitWriter,2);
    //*myInx = atomic_load(&q->dataInx); //get the writer buffer
    *myInx = *myInx & 0x01;


	//FAA(&Buffs[*myInx].nWriter,1); //register writer

    uint32_t inx = FAA(&Buffs[*myInx].writePos,(uint32_t)num); //allocate slots

	//no enough slots left, free back and return NULL
	//It is OK to free back if allocation fail. Because if one writer fail, following writers will also fail.
    if(inx>=nItem)
    {
        FAA(&Buffs[*myInx].writePos,(uint32_t)-num);
        //FAA(&Buffs[*myInx].nWriter,-1); //dec writer.
        Commit(myInx);
        FAA(&q->lostCnt,(uint32_t)num);
        return NULL;
    }

    return Buffs[*myInx].buff+inx;
}



void AddMsg(int data, int dataLen)
{
    uint32_t myInx;
    uint8_t *p = GetSpace(&myInx,dataLen);

    if(p)
    {
        *p = data;
        Commit(&myInx);
    }
}

void WaitDataReady(int inx,bool hotWait)
{
    int index = q->CitWriter & 0x01;
    while(atomic_load(&Buffs[index].nWriter) != atomic_load(&q->CitWriter)>>1)
	{
		// if(hotWait==false)
		// 	SysSleep(1);
	}
    
}

//We assume one reader!!
//Return value is the index to Buffs for read
int Read(bool waitReady,bool hotWait)
{
    uint32_t inx    = q->CitWriter & 0x01;
	uint32_t newInx = inx^1;
	Buffs[newInx].writePos = 0;
    Buffs[newInx].nWriter = Buffs[inx].nWriter;
    __sync_xor_and_fetch(&q->CitWriter,1);
	
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

// void* Producer(void* arg){
//     int data = *(int*)arg;
//     long numMyOps = 0;
//     //---------------------------------
//     //内核绑定
//     // cpu_set_t set;
//     // CPU_ZERO(&set);
//     // CPU_SET(data, &set);
//     // pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
//     //---------------------------------
//     //设置优先级
//     // struct sched_param sp;
//     // bzero((void*)&sp, sizeof(sp));
//     // int policy = sched_get_priority_max(SCHED_FIFO);
//     // sp.sched_priority = policy;
//     // pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
//     //---------------------------------
//     while (run == false){
//         sched_yield();
//     }
    
//     while (stop==false){
//         enqueue(data);
//         numMyOps++;
//     }
//     __sync_fetch_and_add(&NumEnq, numMyOps);

//     printf("Producer ID %d:number of operations is %ld\n",data,numMyOps);
//     return NULL;
// }

// void* Consumer(void* arg){
//     int val = 0;
//     long numMyOps = 0;
//     //---------------------------------
//     //内核绑定
//     // cpu_set_t set;
//     // CPU_ZERO(&set);
//     // CPU_SET(val, &set);
//     // pthread_setaffinity_np(pthread_self(),sizeof(set),&set);
//     //---------------------------------
//     //设置优先级
//     // struct sched_param sp;
//     // bzero((void*)&sp, sizeof(sp));
//     // int policy = sched_get_priority_max(SCHED_FIFO);
//     // sp.sched_priority = policy;
//     // pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
//     //---------------------------------
//     while (run == false){
//         sched_yield();
//     }
   
//     while (stop==false){   
//         val = *(int*)dequeue();
//         num = num + val/4;
//         numMyOps++;
//     }
//     NumDeq = numMyOps;
    
//     printf("Consumer:number of data fetched %ld, last dequeue value is %d\n",num,val);
//     return NULL;
// }

// void testMyqueue(int num_threads){
//     pthread_t threadPool[num_threads];
//     int data[num_threads+1];

//     data[0] = 0;

//     if (num_threads>=1){
//         if(pthread_create(&threadPool[0], NULL, Consumer, NULL) != 0){
//         printf("ERROR, fail to create consumer\n");
//         exit(1);
//         }
//     }
//     else{
//         if(pthread_create(&threadPool[0], NULL, Producer, &data[0]) != 0){
//         printf("ERROR, fail to create producer\n");
//         exit(1);
//         }
//     }
//     for(int i=1;i<num_threads;i++){
//         data[i] = i;
//         if(pthread_create(&threadPool[i], NULL, Producer, &data[i]) != 0){
//         printf("ERROR, fail to create producer\n");
//         exit(1);
//         }
//     }

//     run = true;
//     sleep(running_time);
//     stop = true;
//     for(int i=0;i<num_threads;i++){
//         pthread_join(threadPool[i],NULL);
//     }
    
//     printf("***\ttotal Producer actions are %ld frequency\t***\n",NumEnq/running_time);
//     printf("***\ttotal Consumer actions are %ld frequency\t***\n",NumDeq/running_time);
//     printf("***\ttotal actions are %ld frequency\t***\n",(NumEnq+NumDeq)/running_time);
//     printf("***\tdata throughput capacity are %ld \t***\n",(NumEnq+num)/running_time);
//     if(NumEnq>=num){
//         printf("***\tThe write data failed number is %lu\t***\n",(q->lostCnt/4)/running_time);
//     }
// }

// int main(int argc, char* argv[])
// {
//     void* buffer = malloc(128*1024);
//     Init(buffer, 128*1024);
//     memset(buffer,0,sizeof(128*1024));
//     if (argc < 1)
// 	{
// 		printf(" **********need to specify the amount of threads and running time************* \n");
// 		return 0;
// 	}
//     printf("  Benchmark: %s\n", argv[0]);
    
//     if (argc > 1) {
//         thread_number = atoi(argv[1]);
//     }
//     if (argc > 2) {
//         running_time = atoi(argv[2]);
//     }
//     cpu_num = get_nprocs();
//     printf("  Number of processors: %d\n", thread_number);
    
//     testMyqueue(thread_number);
//     return 0;
// }
