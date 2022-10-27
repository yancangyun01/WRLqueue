//g++ WRL_queue.cpp -g -Wall -O3 -pthread -D_GNU_SOURCE -o output  -pthread -fexceptions
#include <relacy/relacy_std.hpp>


#ifdef NDEBUG
#   define _SECURE_SCL 0
#endif

using namespace std;
using rl::var;

class TWriter2Reader
{
public:
    struct TBuffInfo
    {
        uint8_t  *buff;
        atomic<uint32_t> writePos{0} ;
	atomic<uint32_t> nWriter{0} ;
    } ;

public:
    TBuffInfo Buffs[2]; //The two buffers, one for reader, one for writers
    uint32_t  nItem;    //length of buff for each Buffs
    
    atomic<uint32_t> CitWriter{0}; 
    atomic<uint32_t> lostCnt{0}; //The count for failed writer allocation request.

public:
    // TWriter2Reader()
    // {
    //     Close();
    // }

    void Close()
    {
        memset(static_cast<void*>(this),0,sizeof(TWriter2Reader));
    }

	//Make sure no writer is using the object before call this func!
    void Init(void *buff, uint32_t totalItem)
    {
        //Close();
        nItem = (totalItem>>1);
        
        Buffs[0].buff = (uint8_t*)buff;
        Buffs[1].buff = Buffs[0].buff+nItem;
    }

    uint8_t *GetSpace(uint32_t &myInx,int32_t num)
    {
		myInx = CitWriter($).fetch_add(2);
        myInx = myInx & 0x01;

        uint32_t inx = Buffs[myInx].writePos($).fetch_add(num); //allocate slots

		//no enough slots left, free back and return NULL
		//It is OK to free back if allocation fail. Because if one writer fail, following writers will also fail.
        if(inx>=nItem)
        {
            Buffs[myInx].writePos($).fetch_add(-num);
            Commit(myInx);
            lostCnt($).fetch_add(num);
            return NULL;
        }

        return Buffs[myInx].buff+inx;
    }

    void Commit(uint32_t &myInx)
    {
        while(1){
            uint32_t olddata = CitWriter($).load();
            uint32_t newdata = olddata-2;
            if (myInx != (olddata & 0x01)){  
                Buffs[myInx^1].nWriter($).fetch_add(1);
                return ;
            }
            else if (CitWriter($).compare_exchange_strong(olddata,newdata)){ 
                return ;   
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
        uint32_t inx    = CitWriter($).load() & 0x01;
		uint32_t newInx = inx^1;
		Buffs[newInx].writePos($).store(0); //reset buffer;
        Buffs[newInx].nWriter($) = Buffs[inx].nWriter($).load();
		CitWriter($).fetch_xor(1);

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
		int index = CitWriter($).load() & 0x01;
		
		while(Buffs[index].nWriter($).load() != CitWriter($).load()>>1)
		{
		//	a=Buffs[index].nWriter($).load();
		//	b=CitWriter($).load()>>1;
		}
	}
};


struct WRL_queue_test : rl::test_suite<WRL_queue_test, 4>
{   
    
    void* buffer = malloc(128*1024);
    TWriter2Reader q;
    
    void before()
    {
    	
    	q.Init(buffer, 128*1024);
    	memset(buffer,0,sizeof(128*1024));
    	
    }

    void after()
    {
    	free(buffer);
    }

    void thread(unsigned index)
    {
         if (0 == index)
         {
             q.Read(true,true);
         }
         else
         {
             q.AddMsg(&index,sizeof(index));
         }
    }
    
};

int main()
{
    rl::simulate<WRL_queue_test>();
}
