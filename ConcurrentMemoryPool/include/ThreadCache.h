#ifndef _THREADCACHE_H__
#define _THREADCACHE_H__

#include "Common.h"
#include <cassert>
#include <cstddef>
#include <thread>


class ThreadCache
{
public:
    void *Allocate(size_t size);    //线程申请size大小的空间

    void Deallocate(void* obj,size_t size);     //回收线程中大小为size的obj空间

    //ThreadCache空间不足时，向CentralCache申请空间的接口
    void* FetchFromCentralCache(size_t index,size_t alignSize);


private:
    FreeList _freeLists[FREE_LIST_NUM];   //哈希，每个桶表示一个自由链表

};

//LTS的全局对象的指针，这样每个线程都能有一个独立的全局变量
static __thread ThreadCache* pTLSThreadCache = nullptr; 



#endif