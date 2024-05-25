#pragma once

#include "ThreadCache.h"
#include <cstddef>
#include <thread>


//其实就是tcmalloc，线程调用这个函数申请空间
void *ConcurrentAlloc(size_t size)
{
    std::cout<<"ConcurrentAlloc : "<<std::this_thread::get_id()<<" "<<pTLSThreadCache<<std::endl;
    //pTLSThreadCache是TLS的，每个线程都会有一个，且相互独立，所以不存在竞争pTLSThreadCache的问题，所以只需判断一次就可以new
    if(pTLSThreadCache == nullptr)
    {
        pTLSThreadCache = new ThreadCache;
        //std::cout<<" Construct"<<size<<std::endl;
    }

    //std::cout<<" fenpei"<<size<<std::endl;
    return pTLSThreadCache->Allocate(size);
}

//线程调用这个函数回收空间
void ConcurrentFree(void *obj,size_t size)
    //目前的第二个参数size后面要去掉，这里只是为了代码能跑才给的
{
    assert(obj);

    pTLSThreadCache->Deallocate(obj, size);

}
