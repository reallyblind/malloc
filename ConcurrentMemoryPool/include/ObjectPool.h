#ifndef _OBJECTPOOL_H__
#define _OBJECTPOOL_H__
//定长内存池


#include <iostream>


using std::cout;
using std::endl;


template<class T>
class ObjectPool
{
public:
    T* New()
    {
        T* obj = nullptr;
        if(_freelist)
        {
            void * next = *(void**)_freelist;
            obj = (T*)_freelist;
            _freelist = next;
        }
        else{
            if(_remanentBytes < sizeof(T))
            {
                _remanentBytes = 128*1024;
                _memory = (char*) malloc(_remanentBytes);

                if(_memory == nullptr)
                {
                    throw std::bad_alloc();
                }
            }

            obj = (T*) _memory;

            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
            _memory += objSize;
            _remanentBytes -= objSize;

        }

        
        new (obj)T;
        return obj;
    }


    void Delete(T* obj)
    {
        obj->~T();

        //head insert
        *(void**)obj = _freelist;
        _freelist = obj;
        
    }



private:
    char * _memory = nullptr;   
    void * _freelist = nullptr;
    size_t _remanentBytes = 0;
};



#endif 