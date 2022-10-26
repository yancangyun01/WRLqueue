//---------------------------------------------------------------------------
#ifndef Wr2RdH
#define Wr2RdH
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
    
    volatile uint32_t dataInx; //The index for writer. The index for reader is dataInx^1
    volatile uint32_t lostCnt; //The count for failed writer allocation request.

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
		myInx = AtomicFetch(dataInx); //get the writer buffer

		AtomicInc(Buffs[myInx].nWriter); //register writer

        uint32_t inx = AtomicXchgAdd(Buffs[myInx].writePos,(uint32_t)num); //allocate slots

		//no enough slots left, free back and return NULL
		//It is OK to free back if allocation fail. Because if one writer fail, following writers will also fail.
        if(inx>=nItem)
        {
            AtomicXchgAdd(Buffs[myInx].writePos,(uint32_t)-num);
            AtomicDec(Buffs[myInx].nWriter); //dec writer.
            AtomicXchgAdd(lostCnt,(uint32_t)num);
            return NULL;
        }

        return Buffs[myInx].buff+inx;
    }

    void Commit(uint32_t &myInx)
    {
        AtomicDec(Buffs[myInx].nWriter);
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
        uint32_t inx    = dataInx;
		uint32_t newInx = inx^1;
		Buffs[newInx].writePos = 0; //reset buffer;
		AtomicSwap(&dataInx,newInx); //we use atomic op to make sure cache flush

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
		while(AtomicFetch(Buffs[inx].nWriter)>0)
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
