#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

void* ThreadCache::Allocate(size_t size)    //线程申请size大小的空间
{
    assert(size <= MAX_BYTES);

    size_t alignSize = SizeClass::RoundUp(size);    //size对齐后的字节数
    size_t index = SizeClass::Index(size);      //size对应在哈希表中的哪个桶 3->0 10->1
    //std::cout<<"index: " <<index<<endl;
    if( !_freeLists[index].Empty())
    {
        //自由链表不为空，可以直接从自由链表中获取空间
        std::cout<<"自由链表不为空，可以直接从自由链表中获取空间"<<index<<std::endl;
        return _freeLists[index].Pop();  
    }else {
        //自由链表为空，得要让ThreadCache向CentralCache申请空间
        
        return FetchFromCentralCache(index, alignSize);
    }
    

}

void ThreadCache::Deallocate(void *obj, size_t size)    //回收线程中大小为size的obj空间,size保证是一个对齐后的size
{
    assert(obj);//回收空间不能为空
    assert(size <= MAX_BYTES);//回收空间不能超过256KB

    size_t index = SizeClass::Index(size);  //找到size对应的自由链表
    _freeLists[index].Push(obj);    //用对应自由链表回收空间

}

//ThreadCache空间不足时，向CentralCache申请空间的接口
void* ThreadCache::FetchFromCentralCache(size_t index,size_t alignSize)
{
    //通过MaxSize和NumMoveSize来控制当前给tc提供多少块alignSize大小的空间
    size_t batchNum = std::min(_freeLists[index].MaxSize(),SizeClass::NumMoveSize(alignSize));
    /*
        MaxSize表示index位置的自由链表单次申请未到上限时，能够申请的最大快空间是多少
        NumMoveSize表示tc单次向cc申请alignSize大小的空间块的最多块数是多少
        二者取小，得到的就是本次要给tc提供多少块alignSize大小的空间
        比如alignSize为8B， MaxSize为1 ， NumMoveSize为512，那就给一块8B的空间
        也就是说没到上限就给MaxSize，到了上限就给上限的NumMoveSize
    */

    if(batchNum == _freeLists[index].MaxSize()){
        //如果没有到达上限，那下次再申请这块空间的时候可以多申请一块
        _freeLists[index].MaxSize()++;
        //这里就是慢开始反馈调节的核心
    }

    /* 上面是慢开始反馈调节算法 */

    //输出型参数，返回之后的结果就是tc想要的空间
    void* start = nullptr;
    void* end = nullptr;

    //返回值为实际获取到的块数,  alignSize : size对齐后的字节数
    size_t actulNum = CentralCache::GetInstance()->FetchRangeObj(start,end,batchNum,alignSize);

    assert(actulNum >= 1); //actualNum一定是大于等于1的，这是FetchRangeObj能保证的

    if(actulNum == 1){
        //如果actulNum等于1，就直接将start返回给线程
        assert(start == end);
        return start;
    }
    else {
        //如果actulNum大于1，就还要给tc对应位置插入[Object(start),end]的空间
        _freeLists[index].PushRange(ObjNext(start), end);

        
        #ifdef MYCODE
        ObjNext(start) = nullptr;
        #endif
        
        //给线程返回start所指的空间
        return start;
    }

}


