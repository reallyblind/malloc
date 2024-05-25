#include "../include/CentralCache.h"
#include "../include/ThreadCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <cstddef>
#include <cstdio>

CentralCache CentralCache::_sInst; //CentralCache的饿汉对象

//cc从自己的_spanLists中为tc提供tc所需要的块空间
size_t CentralCache::FetchRangeObj(void* &start, void*& end, size_t batchNum,size_t size)
{
    //获取到size对应哪一个SpanList
    size_t index = SizeClass::Index(size);

    _spanLists[index]._mtx.lock();
    

    //获取到一个管理空间非空的span
    Span* span = GetOneSpan(_spanLists[index], size);
    std::cout<<"return span"<<span<<endl;
    assert(span);//断言一下span不为空
    assert(span->_freeList);////断言一下span管理的空间不为空
    

    //起初都指向_freeList，让end不断往后走
    start = end = span->_freeList;
    size_t actualNum = 1; //函数实际的返回值

    //在end的next不为空的前提下，让end走batchNum-1步
    std::cout<<"the point of "<<span->_freeList<<endl;

    size_t i = 0;
    std::cout<<"end = "<<end<<endl;
    while(i < batchNum - 1 && ObjNext(end) != nullptr)
    {
        end = ObjNext(end);
        ++actualNum; //记录end走过了多少步
        ++i; //控制条件
    }

    //将[start , end]返回给ThreadCache后，调整Span的_freeList
    span->_freeList = ObjNext(end);
    //返回一段空间，不要和原先Span的_freeList中的块相连
    ObjNext(end) = nullptr;

    _spanLists[index]._mtx.unlock();

    return actualNum;

}

//获取一个管理空间不为空的span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
    //先在cc中找一下有没有管理空间非空的span
    Span* it = list.Begin();
    while(it != list.End())
    {
        if(it->_freeList != nullptr)    //找到管理空间非空的span
            return it;
        else //没找到继续往下找
            it = it->_next;
    }
    //解掉桶锁，让其他向该cc桶进行操作的线程拿到锁
    list._mtx.unlock();

    //走到这里就是cc中没有找到管理空间非空的span
    
    //将size转换成匹配的页数，以供pc提供一个合适的span
    size_t k = SizeClass::NumMovePage(size);

    
    size_t size_duiqi = SizeClass::NumMoveSize(size);

    //解决死锁的方法3:在调用NewSpan的地方加锁
    PageCache::GetInstance()->_pageMtx.lock();
    //调用NewSpan获取一个全新的span
    Span* span = PageCache::GetInstance()->NewSpan(k);
    PageCache::GetInstance()->_pageMtx.unlock();

    //这样就能让CentralCache从PageCache中获取到一个没有划分过的全新span，注意我加粗了没有划分过，前面说了PageCache中的span都是未划分过的span，
    //所以CentralCache获取到这些未划分的span之后需要自己根据size划分一下这些span，那么怎么划分呢？

    //首先要拿到span所管理空间的首地址，可以直接通过页号来获取，即 页号 * 单页大小（或者页号左移PAGE_SHIFT） 即为span所管理页的首地址（后续会再NewSpan中保证这一点，不懂的同学暂时记下来）。

    //NewSpan中实现的时候会设置好返回的span中的_pageID和_n，也就是页号和管理页数。其中的其他项都是默认值，比如_freeList是空的。

    //首地址start = 页号 * 单页大小：
    //通过页数算出总空间大小 = 页数 * 页大小，start加上总空间大小就是end：
    //再通过size来划分块：

    //这里要强转一下，因为_pageID是PageID类型(size_t或者unsigned long long)的，不能直接赋值给指针
    char* start = (char*)(span->_pageID << PAGE_SHIFT);
    char* end = (char*)(start + (span->_n << PAGE_SHIFT));

    //开始切分span管理的空间
    span->_freeList = start; //管理的空间放到span->_freeList中

    void* tail = start; //起初让tail指向start
    start += size; //start往后移动一块，方便控制循环



cout << "span->_pageID: " << span->_pageID << endl;
cout << "span->_pageID << PAGE_SHIFT: " << (span->_pageID << PAGE_SHIFT) << endl;
cout << "start address: " << static_cast<void*>(start) << endl;
cout << "end address: " << static_cast<void*>(end) << endl;

cout<<"----------------"<<endl;
    cout<<"span from Page:"<<span<< "  :  "<<span->_pageID<< " == "<<(span->_pageID << PAGE_SHIFT)<<"?"<<((span->_pageID << PAGE_SHIFT)==span->_pageID *8*1024 ?"true":"false" )<< endl;
    cout<<"span->_freeList" << span->_freeList<<endl;
    cout<<"span->_freeList" << span->_next<<endl;
    printf("%p\n",start  );
    printf("%p\n",end  );
    cout << "span: " << span << endl;
    int i = 0;
    std::cerr << "Error: start address is not less than end address." << std::endl;
    if (start <= end) {
        std::cerr << "Error: start address is not less than end address." << std::endl;
    }
    int* tmp = (int*)tail;
    printf("tail = %p\n",tail);
    printf("(int*)tail = %p\n",tmp);
    printf("(int*)tail = %d\n",*tmp);
    printf("ObjNext(tail) = %p\n",*(void**)tail);
    //链接各个块
    while(start < end)
    {
        ObjNext(tail) = start;
        start += size;
        tail = ObjNext(tail);
    }
    
    ObjNext(tail) = nullptr;    //记得把最后一位置空

    //切好span后，需要把span挂到cc对应下标的桶里面去
    list._mtx.lock(); //span挂上去之前加锁
    list.PushFront(span);

    return span;
}


