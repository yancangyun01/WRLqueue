//---------------------------------------------------------------------------
#ifndef Wr2Rd_2H
#define Wr2Rd_2H
//---------------------------------------------------------------------------
#include <string.h>
#include "SyncAtomic.h"
#include "align.h"
//---------------------------------------------------------------------------
#pragma pack(push,1)
/*---------------------------------------------------------------------------
Multi-writer-one-reader cycle buffer implementation.
This class uses a lock free and semi block free algorithm.
1. Buffer is split to two equal part, one used for writers, one used for reader.
   As a result, both reader and writers are able to operate with lock free algorithm.
2. The writers are block free, i.e. no wait (hot spin or semaphore wait) at all.
3. The reader switch the two halves and wait for all writers in old writer half to exit.
   As a result, the reader may block for a while. This implementation uses a hot spin.
   So it may NOT work if the reader has higher previlige than some writer. If reader
   can be interrupted by writer, then in theory the reader will not block forever.
4. Writers will fail when the write-half buffer is full.
---------------------------------------------------------------------------*/
class TWriter2Reader
{
public:
    struct TBuffInfo
    {
        uint8_t          *buff;
        volatile uint32_t writePos DOUBLE_CACHE_ALIGNED;
		volatile uint32_t nWriter DOUBLE_CACHE_ALIGNED;
    } DOUBLE_CACHE_ALIGNED;

public:
    TBuffInfo Buffs[2]; //The two buffers, one for reader, one for writers
    uint32_t  nItem;    //length of buff for each Buffs
    
    //volatile uint32_t dataInx; //The index for writer. The index for reader is dataInx^1
    volatile uint32_t lostCnt; //The count for failed writer allocation request.
    volatile uint64_t CitWriter CACHE_ALIGNED;

public:
    TWriter2Reader()
    {
        Close();
    }

    void Close()
    {
        memset(this,0,sizeof(TWriter2Reader));
    }

	//Make sure no writer is using the object before call this func!
    void Init(void *buff, uint32_t totalItem)
    {
        Close();
        nItem = (totalItem>>1);
        
        Buffs[0].buff = (uint8_t*)buff;
        Buffs[1].buff = Buffs[0].buff+nItem;
    }

    uint8_t *GetSpace(uint32_t &myInx,int32_t num)
    {
        //nWriter每次+2
        //myInx = AtomicFetch(dataInx); //get the writer buffer

	    myInx = AtomicXchgAdd(CitWriter,(uint64_t)2); //register writer
        myInx = myInx & 0x01;
        //此处不妨也令nWriter自增，以此来降低写的速率，防止出现大量写失败
        //AtomicInc(Buffs[myInx].nWriter);
        uint32_t inx = AtomicXchgAdd(Buffs[myInx].writePos,(uint32_t)num); //allocate slots

		//no enough slots left, free back and return NULL
		//It is OK to free back if allocation fail. Because if one writer fail, following writers will also fail.
        if(inx>=nItem)
        {
            AtomicXchgAdd(Buffs[myInx].writePos,(uint32_t)-num);
            Commit(myInx);
            AtomicXchgAdd(lostCnt,(uint32_t)num);
            return NULL;
        }

        return Buffs[myInx].buff+inx;
    }

    void Commit(uint32_t &myInx)
    {
        while(1){
            //此处设置if语句，使得从开始就对写指针位置进行判断，若还在则-2，否则另一个区域的nWriter+1，在该过程中read不可能连续改变两次
            uint64_t olddata = CitWriter;
            uint64_t newdata = olddata-2;
            if (myInx != (olddata & 0x01)){  
                AtomicInc(Buffs[myInx^1].nWriter);
                return ;
            }
            else if (AtomicCmpXchg(&CitWriter,newdata,olddata)){ 
                
                // AtomicXchgAdd(CitWriter,(uint64_t)-2);
                // //若前面nWriter增加了，则此处要减少
                //AtomicDec(Buffs[myInx].nWriter);
                return ;
                //使用compare and swap语句，由于其过程中CitWriter会发生变化，为保证成功需要自旋，如果不自旋则会导致CitWriter计数错误
                // int olddata = CitWriter;
                // int newdata = olddata-2;
                // while(!AtomicCmpXchg(&CitWriter,newdata,olddata)){
                //    olddata = CitWriter;
                //    newdata = olddata-2;
                // }     
            }
        }
        
    }

    void AddMsg(const void *data, int dataLen)
    {
        uint32_t myInx;
        uint8_t *p = GetSpace(myInx,dataLen);

        if(p)
        {
            memcpy(p,data,dataLen);
            Commit(myInx);
        }
    }

    //We assume one reader!!
	//Return value is the index to Buffs for read
    int Read(bool waitReady=true,bool hotWait=true)
    {
        uint32_t inx    = CitWriter & 0x01;
		uint32_t newInx = inx^1;
		Buffs[newInx].writePos = 0; //reset buffer;
        //若上述nWriter没有自增自减，则此处需要该语句，使得nWriter与CitWriter次数对应上，否则会死循环
        Buffs[newInx].nWriter = Buffs[inx].nWriter;
		//we use atomic op to make sure cache flush
        AtomicXor(CitWriter,(uint64_t)1);

		if(waitReady)
		{
			WaitDataReady(inx,hotWait);
		}

        return inx;
    }

	TBuffInfo &ReadData(bool waitReady=true,bool hotWait=true)
	{
		return Buffs[Read(waitReady,hotWait)];
	}

	void WaitDataReady(int inx,bool hotWait=false)
	{
		//Both hot spin to wait or delay to wait may NOT work on some system!
        int index = CitWriter & 0x01;
		while(AtomicFetch(Buffs[index].nWriter) != CitWriter>>1)
		{
			if(hotWait==false)
				SysSleep(1);
		}
	}
};
//---------------------------------------------------------------------------
#pragma pack(pop)
//---------------------------------------------------------------------------
#endif //Exclusive Include
