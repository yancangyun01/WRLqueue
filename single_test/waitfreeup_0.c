#include "time.h"
#include "Wr2Rdup.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <stdbool.h>
#include <stdatomic.h>

TBuffInfo Buffs[2]; //The two buffers, one for reader, one for writers
static uint32_t  nItem;    //length of buff for each Buffs
static record_t * q;
//-------------------------------------------------------
int testnum = 2000;
//-------------------------------------------------------
//Make sure no writer is using the object before call this func!
void Init(void *buff, uint32_t totalItem)
{
    nItem = (totalItem>>1);
    q = align_malloc(PAGE_SIZE, sizeof(record_t));    
    Buffs[0].buff = (uint8_t*)buff;
    Buffs[1].buff = Buffs[0].buff+nItem;
}

void Commit(uint32_t *myInx)
{
    //FAA(&Buffs[*myInx].nWriter,-1);

    if (*myInx != (q->CitWriter & 0x01)){  
        FAA(&Buffs[*myInx^1].nWriter,1);
    }
    else{
        FAA(&q->CitWriter,-2);
        FAA(&Buffs[*myInx].nWriter,-1);  
    }
}

uint8_t *GetSpace(uint32_t *myInx,int32_t num)
{
	FAA(&q->CitWriter,2);
    //*myInx = atomic_load(&q->dataInx); //get the writer buffer
    *myInx = q->CitWriter & 0x01;


	FAA(&Buffs[*myInx].nWriter,1); //register writer

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
	//Both hot spin to wait or delay to wait may NOT work on some system!
	//while(FAA(&Buffs[inx].nWriter,0)>0)
    //while(atomic_load(&Buffs[inx].nWriter)>0)
    //--------------------------------------------
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
    //uint32_t inx    = q->dataInx;
	//uint32_t newInx = inx^1;
	//Buffs[newInx].writePos = 0; //reset buffer;
	//atomic_store(&q->dataInx,newInx); //we use atomic op to make sure cache flush
    //---------------------------
    uint32_t inx    = q->CitWriter & 0x01;
	uint32_t newInx = inx^1;
	Buffs[newInx].writePos = 0;
    if(inx == 0){
        FAA(&q->CitWriter,1);
    }
    else{
        FAA(&q->CitWriter,-1);
    }
	
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


int main(int argc, char* argv[])
{
    void* buffer = malloc(128*1024);
    Init(buffer, 128*1024);
    
    if (argc > 1) {
        testnum = atoi(argv[1]);
    }

    uint64_t begin;
    uint64_t end;
    uint64_t mean_w=0,max_w=0,mean_r=0,max_r=0;
    
    for (int i=0;i<testnum;i++){
        begin = rdtsc();

        enqueue(i);

        end = rdtsc();
        
        mean_w =mean_w + (end - begin);
        if(max_w<(end - begin)) max_w = (end - begin);
    
    }

    for (int i=0;i<testnum;i++){
        begin = rdtsc();

        dequeue();

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
