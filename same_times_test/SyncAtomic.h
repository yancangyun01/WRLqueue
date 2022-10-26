//---------------------------------------------------------------------------
#ifndef SyncAtomicH
#define SyncAtomicH
//---------------------------------------------------------------------------
#include <stdint.h>
#include "AutoLocker.h"
//---------------------------------------------------------------------------
#pragma pack(push,1)
//---------------------------------------------------------------------------
#ifdef __GNUC__
//---------------------------------------------------------------------------
template<class tvalue>
inline tvalue AtomicSwap(tvalue *dest,tvalue value)
{
    return __sync_lock_test_and_set( dest, value );
}
//---------------------------------------------------------------------------
template<class tvalue>
inline tvalue AtomicSwap(volatile tvalue *dest,tvalue value)
{
    return __sync_lock_test_and_set( (tvalue *)dest, value );
}
//---------------------------------------------------------------------------
template<class tvalue>
inline bool AtomicCmpXchg(tvalue *dest,tvalue newVal,tvalue oldVal)
{
    return __sync_bool_compare_and_swap(dest,oldVal,newVal);
}
//---------------------------------------------------------------------------
template<class tvalue>
inline bool AtomicCmpXchg(volatile tvalue *dest,tvalue newVal,tvalue oldVal)
{
    return __sync_bool_compare_and_swap((tvalue *)dest,oldVal,newVal);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicXchgAdd(tint &count,tint delta)
{
    return __sync_fetch_and_add(&count,delta);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicXchgAdd(volatile tint &count,tint delta)
{
    return __sync_fetch_and_add((tint *)&count,delta);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicAnd(volatile tint &dest,tint value)
{
    return __sync_fetch_and_and((tint *)&dest,value);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicAnd(tint &dest,tint value)
{
    return __sync_fetch_and_and(&dest,value);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicOr(tint &dest,tint value)
{
    return __sync_fetch_and_or(&dest,value);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicOr(volatile tint &dest,tint value)
{
    return __sync_fetch_and_or((tint *)&dest,value);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicXor(tint &dest,tint value)
{
    return __sync_fetch_and_xor(&dest,value);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicXor(volatile tint &dest,tint value)
{
    return __sync_fetch_and_xor((tint *)&dest,value);
}
//---------------------------------------------------------------------------
#else //__GNUC__
//---------------------------------------------------------------------------
int32_t AtomicSwap(int32_t *dest,int32_t value);
int64_t AtomicSwap(int64_t *dest,int value);
//---------------------------------------------------------------------------
inline uint32_t AtomicSwap(uint32_t *dest, uint32_t value)
{
    return (uint32_t)AtomicSwap((int32_t *)dest,(int32_t)value);
}
//---------------------------------------------------------------------------
inline uint64_t AtomicSwap(uint64_t *dest, uint64_t value)
{
    return (uint64_t)AtomicSwap((int64_t *)dest,(int64_t)value);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicSwap(volatile tint *dest, tint value)
{
    return AtomicSwap((tint *)dest,value);
}
//---------------------------------------------------------------------------
inline void *AtomicSwap(void **dest,void *value)
{
#ifdef _WIN64
    return (void *)AtomicSwap( (int64_t*)dest, (int64_t)value );
#else
    return (void *)AtomicSwap( (int32_t*)dest, (int32_t)value );
#endif
}
//---------------------------------------------------------------------------
bool  AtomicCmpXchg(int32_t *dest,int32_t newVal,int32_t oldVal);
bool  AtomicCmpXchg(int64_t *dest,int64_t newVal,int64_t oldVal);
//---------------------------------------------------------------------------
inline bool AtomicCmpXchg(uint32_t *dest, uint32_t newVal, uint32_t oldVal)
{
    return AtomicCmpXchg((int32_t *)dest,(int32_t)newVal,(int32_t)oldVal);
}
//---------------------------------------------------------------------------
inline bool AtomicCmpXchg(uint64_t *dest, uint64_t newVal, uint64_t oldVal)
{
    return AtomicCmpXchg((int64_t *)dest,(int64_t)newVal,(int64_t)oldVal);
}
//---------------------------------------------------------------------------
template<class tint>
inline bool AtomicCmpXchg(volatile tint *dest, tint newVal, tint oldVal)
{
    return AtomicCmpXchg((tint *)dest,newVal,oldVal);
}
//---------------------------------------------------------------------------
//The return value is the OLD count value (before inc/dec/add)
int32_t AtomicXchgAdd(int32_t &count,int32_t delta);
int64_t AtomicXchgAdd(int64_t &count,int64_t delta);
//---------------------------------------------------------------------------
inline uint64_t AtomicXchgAdd(uint64_t &dest, uint64_t delta)
{
    return (uint64_t)AtomicXchgAdd(*(int64_t *)&dest,(int64_t)delta);
}
//---------------------------------------------------------------------------
inline uint32_t AtomicXchgAdd(uint32_t &dest, uint32_t delta)
{
    return (uint32_t)AtomicXchgAdd(*(int32_t *)&dest,(int32_t)delta);
}
//---------------------------------------------------------------------------
template<class tint>
inline tint AtomicXchgAdd(volatile tint &dest, tint delta)
{
    return AtomicXchgAdd(*(tint *)&dest,delta);
}
//---------------------------------------------------------------------------
int AtomicAnd(int &dest,int value);
int AtomicOr(int &dest,int value);
int AtomicXor(int &dest,int value);
//---------------------------------------------------------------------------
inline uint32_t AtomicAnd(uint32_t &dest,uint32_t value)
{
    return (uint32_t)AtomicAnd(*(int32_t *)&dest,(int32_t)value);
}
//---------------------------------------------------------------------------
inline uint32_t AtomicOr(uint32_t &dest,uint32_t value)
{
    return (uint32_t)AtomicOr(*(int32_t *)&dest,(int32_t)value);
}
//---------------------------------------------------------------------------
inline uint32_t AtomicXor(uint32_t &dest,uint32_t value)
{
    return (uint32_t)AtomicXor(*(int32_t *)&dest,(int32_t)value);
}
//---------------------------------------------------------------------------
#endif//__GNUC__
//---------------------------------------------------------------------------
//For any var larger than sizeof(register), this should be used to read it out.
//Or else the read out value may be inconsistent.
//---------------------------------------------------------------------------
template<class tint> inline tint AtomicFetch(tint &count)
{
    return AtomicXchgAdd(count,(tint)0);
}
//---------------------------------------------------------------------------
template<class tint> inline tint AtomicInc(tint &count)
{
    return AtomicXchgAdd(count,(tint)1);
}
//---------------------------------------------------------------------------
template<class tint> inline tint AtomicDec(tint &count)
{
    return AtomicXchgAdd(count,(tint)-1);
}
//---------------------------------------------------------------------------------
template<class tint> inline void AtomicMax(volatile tint &var,tint nowVal)
{
    while(1)
    {
        tint s = AtomicFetch(var);
        if(nowVal>s)
        {
            if(AtomicCmpXchg((tint*)&var,nowVal,s)==false)
                continue;
        }

        break;
    }
}
//---------------------------------------------------------------------------
//Atomic max with limit to (mask+1), used for index of cycle buffers
//Causion 1: mask must be in format of ((1<<x)-1), that is, a low bits mask.
//Causion 2: cycle-back decision point is (mask>>1), that is,
//    only if nowVal<=(var+mask>>1) can it be taken as a larger value.
//    As a result, the buffer should only be half filled.
template<class tint> inline void AtomicMaxLimit(volatile tint &var,tint nowVal,tint mask)
{
    nowVal &= mask;
    while(1)
    {
        tint s = AtomicFetch(var);
        if( ((nowVal-s)&mask) <= (mask>>1) )
        {
            if(AtomicCmpXchg((tint*)&var,nowVal,s)==false)
            {
                continue;
            }
            s = 0;
        }

        break;
    }
}
//----  --------------------------------------------------------------------------------------------------------
void SysSleep(uint32_t ms);
//---------------------------------------------------------------------------
#pragma pack(pop)
//---------------------------------------------------------------------------
#endif
