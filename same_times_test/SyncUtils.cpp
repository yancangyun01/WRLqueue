//---------------------------------------------------------------------------
#include <stdint.h>
//---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64) || defined(__WIN32__)
//---------------------------------------------------------------------------
#include <windows.h>
//---------------------------------------------------------------------------
void  SysSleep(unsigned ms)
{
    Sleep(ms);
}
//---------------------------------------------------------------------------
#else //_Windows
//---------------------------------------------------------------------------
#include <time.h>
void  SysSleep(unsigned ms)
{
    struct timespec tv;
    tv.tv_sec = ms/1000;
    tv.tv_nsec = (ms%1000)*1000000;
    nanosleep(&tv,NULL);
    //taskDelay(ms/msPerTick);
}
//---------------------------------------------------------------------------
/*
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
//---------------------------------------------------------------------------
void  SysSleep(unsigned ms)
{
	struct timeval tv;

	if(ms==0)
		ms = 1; // we do not know whether Linux supports 0
	tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000)*1000;
    select (0, NULL, NULL, NULL, &tv);
}
*/
//---------------------------------------------------------------------------
#endif //_Windows
//---------------------------------------------------------------------------
// For GNU C, we use the primitive always. For Windows, we may use the system
// calls if GNU primitives are unavailable. Please note on X64, void * has
// different size (8 bytes) than int (4 bytes).
//---------------------------------------------------------------------------
#ifdef __GNUC__
//---------------------------------------------------------------------------
// Already in header file.
//---------------------------------------------------------------------------
#elif  defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) //__GNUC__
//---------------------------------------------------------------------------
#ifdef __BORLANDC__
extern "C"
{
int64_t InterlockedCompareExchange64(int64_t volatile *Destination, int64_t ExChange, int64_t Comperand);
int64_t InterlockedExchange64( int64_t *dest, int64_t value );
}
#endif
//---------------------------------------------------------------------------
void *AtomicSwap(void **dest,void *value)
{
#ifdef _WIN64
    return (void *)InterlockedExchange64( (LONG64*)dest, (LONG64)value );
#else
    return (void *)InterlockedExchange( (LPLONG)dest, (LONG)value );
#endif
}
//---------------------------------------------------------------------------
int AtomicSwap(int *dest,int value)
{
    return InterlockedExchange( (LPLONG)dest, (LONG)value );
}
//---------------------------------------------------------------------------
bool AtomicCmpXchg(void **dest,void *newVal,void *oldVal)
{
#ifdef _WIN64
    return InterlockedCompareExchange64((int64_t*)dest,(int64_t)newVal,(int64_t)oldVal)==(int64_t)oldVal;
#else
    return InterlockedCompareExchange((LONG*)dest,(LONG)newVal,(LONG)oldVal)==(LONG)oldVal;
#endif
}
//---------------------------------------------------------------------------
bool AtomicCmpXchg(int32_t *dest,int32_t newVal,int32_t oldVal)
{
    return InterlockedCompareExchange((LONG*)dest,(LONG)newVal,(LONG)oldVal)==(LONG)oldVal;
}
//---------------------------------------------------------------------------
bool AtomicCmpXchg(int64_t *dest,int64_t newVal,int64_t oldVal)
{
    return InterlockedCompareExchange64(dest,newVal,oldVal)==oldVal;
}
//---------------------------------------------------------------------------
int AtomicXchgAdd(int &count,int delta)
{
    return InterlockedExchangeAdd((PLONG)&count,delta);
}
//---------------------------------------------------------------------------
int64_t AtomicXchgAdd(int64_t &count,int64_t delta)
{
#ifdef __BORLANDC__
    int64_t volatile* pc = (int64_t volatile*)&count;
    int64_t oldVal, newVal;
    do
    {
        oldVal = *pc;
        newVal = oldVal+delta;
    }while(InterlockedCompareExchange64(pc,newVal,oldVal)!=oldVal);
    return newVal;
#else
    return InterlockedExchangeAdd64((int64_t *)&count,delta);
#endif
}
//---------------------------------------------------------------------------
int AtomicAnd(int &dest,int value)
{
	while(1)
	{
		int oldVal = dest;
		int newVal = oldVal & value;
		if(AtomicCmpXchg(&dest,newVal,oldVal))
			return oldVal;
	}
}
//---------------------------------------------------------------------------
int AtomicOr(int &dest,int value)
{
	while(1)
	{
		int oldVal = dest;
		int newVal = oldVal | value;
		if(AtomicCmpXchg(&dest,newVal,oldVal))
			return oldVal;
	}
}
//---------------------------------------------------------------------------
int AtomicXor(int &dest,int value)
{
	while(1)
	{
		int oldVal = dest;
		int newVal = oldVal ^ value;
		if(AtomicCmpXchg(&dest,newVal,oldVal))
			return oldVal;
	}
}
//---------------------------------------------------------------------------
#endif //__GNUC__
