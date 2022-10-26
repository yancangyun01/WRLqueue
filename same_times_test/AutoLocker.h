//----  --------------------------------------------------------------------------------------------------------
#ifndef AutoLockerH
#define AutoLockerH
//----  --------------------------------------------------------------------------------------------------------
template<class T>
class TAutoLocker
{
protected:
    T &obj;

public:
    TAutoLocker(T &myObj)
    :obj(myObj)
    {
        obj.Lock();
    }

    ~TAutoLocker()
    {
        obj.Unlock();
    }
};
//----  --------------------------------------------------------------------------------------------------------
template<class T>
class TAutoHotLocker
{
protected:
    T &obj;

public:
    TAutoHotLocker(T &myObj)
    :obj(myObj)
    {
        obj.HotLock();
    }

    ~TAutoHotLocker()
    {
        obj.Unlock();
    }
};
//----  --------------------------------------------------------------------------------------------------------
template<class T>
class TAutoTryLocker
{
protected:
    T *obj;

public:
    TAutoTryLocker(T &myObj)
    {
        if(myObj.TryLock())
			obj = &myObj;
		else
			obj = NULL;
    }

	bool operator()()
	{
		return obj!=NULL;
	}

    ~TAutoTryLocker()
    {
		if(obj)
			obj->Unlock();
    }
};
//----  --------------------------------------------------------------------------------------------------------
#endif
