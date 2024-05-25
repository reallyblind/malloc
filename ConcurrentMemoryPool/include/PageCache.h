#pragma once

#include "Common.h"
#include <cstddef>
#include <mutex>


class PageCache{
public:
    static PageCache* GetInstance(){
        return &_sInst;
    }

    //pc从_spanLists中拿出来一个k页的span
    Span* NewSpan(size_t k);
    
    std::mutex _pageMtx; //pc整体的锁
private:
    SpanList _spanLists[PAGE_NUM]; //pc中的哈希
    

private:
    PageCache(){}
    PageCache(const PageCache& copy) = delete;
    PageCache& operator=(const PageCache&copy) = delete;

    static PageCache _sInst; //单例类对象

};

