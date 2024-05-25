#ifndef _COMMON_H__
#define _COMMON_H__

#include <cstddef>
#include <iostream>
#include <new>
#include <vector>
#include <mutex>
#include<unistd.h>
#include <cassert>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#define MYCODE

using std::vector;
using std::cout;
using std::endl;

typedef size_t PageID;

//一个span 256KB
//1页8KB
//128页的span
//128页的span是直接用系统接口申请的，那就是连续的空间。
static const size_t FREE_LIST_NUM = 208;    //哈希表中自由链表的个数
static const size_t MAX_BYTES = 256 * 1024 ;    //ThreadCache单次申请的最大字节数256k
static const size_t PAGE_NUM = 129; //span的最大管理页数
static const size_t PAGE_SHIFT = 13; //一页多少位，这里给1页8KB，就是13位

static void* _SystemAlloc(size_t kpage)
{
    #ifdef _WIN32
    void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    //linux
    void* ptr;
    size_t size = kpage << 13;
    const size_t THRESHOLD = 128 * 1024; // 128 KB threshold for switching between brk and mmap

    if (size < THRESHOLD) {
        // Use brk for small allocations
        static void* brk_base = sbrk(0); // Initialize base address if not set
        void* requested = (void*)((char*)brk_base + size);
        std::cout << "brk request allocated at " << requested << std::endl;
        if (sbrk(size) == (void*)-1) {
            ptr = nullptr;
        } else {
                 ptr = brk_base;
        std::cout << "ptr allocated at " << ptr << std::endl;
            brk_base = requested;
        std::cout << "brk_base allocated at " << brk_base << std::endl;
        }
    } else {
        // Use mmap for large allocations
        //ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,  MAP_SHARED|MAP_ANONYMOUS, -1, 0);
        std::cout << "mmap allocated at " << ptr << std::endl;
        if (ptr == MAP_FAILED) {
            ptr = nullptr;
        }
    }


#endif
    if(ptr == nullptr)
        throw std::bad_alloc();
    return ptr;
}

inline static void* SystemAlloc(size_t kpage)
{
    void* ptr;
    size_t size = kpage << 13;
    ptr = malloc(size);
    if(ptr == NULL) 
        return nullptr;
    memset(ptr,0,size);
    printf("malloc %p\n",ptr);
    return ptr;
}


static void*& ObjNext(void* obj) //obj的头4/8个字节
{
    return *(void**)obj;
}


//ThreadCache
class FreeList
{
public:

    //向自由链表头插，且插入多块空间
    void PushRange(void* start, void* end)
    {
        ObjNext(end) = _freeList;
        _freeList = start;
    }

    bool Empty()//判断哈希桶是否为空, 即链表是否为空
    {
        return _freeList == nullptr;
    }

    void Push(void * obj)   //回收空间
    {
        assert(obj);    //插入非空空间

        //*(void**)obj = _freeList;
        ObjNext(obj) = _freeList;

        _freeList = obj;
    }
    void *Pop() //提供空间
    {
        assert(_freeList);  //提供空间的前提是要有空间

        void *obj = _freeList;
        _freeList = ObjNext(obj);

        return obj;
    }

    //FreeList当前未到上限时，能够申请的最大快空间是多少
    size_t& MaxSize(){
        return _maxSize;
    }

private:
    void * _freeList = nullptr; //自由链表，初始为空
    size_t _maxSize = 1;//当前自由链表申请未达到上限时，能够申请的最大快空间是多少，初始值为1，表示第一次能申请的就是1块，到了上限后_maxSize这个值就作废了


};

//CentralCache
struct Span{
public:
    PageID _pageIDleft = 0;//
    PageID _pageID = 0; //页号
    size_t _n = 0;//当前span管理的页的数量

    void* _freeList = nullptr;//每个span下面挂的小块空间的头结点
    size_t use_count = 0;//当前span分配出去了多少个块空间

    Span* _prev = nullptr;
    Span* _next = nullptr;

};
//CentralCache
class SpanList{
public:
    std::mutex _mtx;

    Span* PopFront()
    {
        //先获取到_head后面的第一个span
        Span* front = _head->_next;

        //删掉这个span，直接复用Erase
        Erase(front);

        //返回原来的第一个span
        return front;
    }

    //判空
    bool Empty()
    {
        //带头双向循环空的时候_head指向自己
        return _head == _head->_next;
    }

    //头插
    void PushFront(Span* span)
    {
        Insert(Begin(), span);
    }


    //头结点
    Span* Begin()
    {
        return _head->_next;
    }

    //尾节点
    Span* End()
    {
        return _head;
    }

    SpanList()
    {
        //构造函数中构造哨兵位头结点
        _head = new Span;

        _head->_next = _head;
        _head->_prev = _head;
    }

    void Insert(Span* pos, Span* ptr)
    {
        //在pos前面插入ptr
        assert(pos);
        assert(ptr);

        //插入相关逻辑
        Span* prev = pos->_prev;

        prev->_next = ptr;
        ptr->_prev = prev;

        ptr->_next = pos;
        pos->_prev = ptr;
    }

    void Erase(Span *pos)
    {
        assert(pos);
        assert(pos != _head);

        Span* prev = pos->_prev;
        Span* next = pos->_next;

        //不删除因为只是回收，不是直接删掉
    }

private:
    Span* _head;//哨兵位头结点
    
};



class SizeClass
{
public:
    //单次申请上限的算法
    /*
        MAX_BYTES / size;
    */
    static size_t NumMoveSize(size_t size)
    {
        assert(size > 0);//不能申请0大小的空间

        //MAX_BYTES就是单个块的最大空间，也就是256k
        int num = MAX_BYTES / size; //这里除之后先简单控制一下
        if(num > 512){
            //比如说单次申请的是8B，256KB除以8B得到的是一个3万多的数，那这样单次上限3万多块太多了，直接开到3万多可能造成很多的浪费，不太现实，所以应该小一点
            num = 512;
        }

        //如果说除了之后特别小，比2小，那么就调成2
        if(num < 2){
            //比如说单次申请的是256KB，那除得1，如果256KB上限一直是1，那这样有点太少了，可能线程要的是4个256KB，那将num改成2就可以少调用几次，
            //也就会少几次开销，但也不能太多，256KB空间是很大的，num太高了不太现实，可能出现浪费

            num = 2;
        }
        //[2,512],一次批量移动多少个对象的（慢启动）上限值
        //小对象一次批量上限高
        //小对象一次批量上限低

        return num;
    }

    static size_t RoundUp(size_t size)
    {
        if(size <= 128)                 //[1,128]  8B
            return _RoundUp(size, 8);
        else if(size <= 1024)           //[128+1,1024]  16B
            return _RoundUp(size, 16);
        else if(size <= 8*1024)         //[1024+1,8*1024]  128B
            return _RoundUp(size, 128);
        else if(size <= 64*1024)        //[8*1024+1,64*1024]  1024B
            return _RoundUp(size, 1024);
        else if(size <= 256*1024)       //[64*1024+1,256*1024]  8 * 1024B
            return _RoundUp(size, 8*1024);
        else{
            assert(false);
            return -1;
        } 
    }

    static size_t _RoundUp(size_t size,size_t alignNum)
    {
        //计算每个分区对应的对齐后的字节数
        size_t res = 0;
        if(size % alignNum != 0){
            //size = 11   (11/8+1)*8 = 16;
            res = (size / alignNum +1) *alignNum;
        }else {
            //size = 16   
            res = size;
        }
        return res;
    }

    // 求size对应在哈希表中的下标
static inline size_t _Index(size_t size, size_t align_shift)
{							/*这里align_shift是指对齐数的二进制位数。比如size为2的时候对齐数
							为8，8就是2^3，所以此时align_shift就是3*/
	return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
	//这里_Index计算的是当前size所在区域的第几个下标，所以Index的返回值需要加上前面所有区域的哈希桶的个数
}

// 计算映射的哪一个自由链表桶
static inline size_t Index(size_t size)
{
	assert(size <= MAX_BYTES);

	// 每个区间有多少个链
	static int group_array[4] = { 16, 56, 56, 56 };
	if (size <= 128)
	{ // [1,128] 8B -->8B就是2^3B，对应二进制位为3位
		return _Index(size, 3); // 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
	}
	else if (size <= 1024)
	{ // [128+1,1024] 16B -->4位
		return _Index(size - 128, 4) + group_array[0];
	}
	else if (size <= 8 * 1024)
	{ // [1024+1,8*1024] 128B -->7位
		return _Index(size - 1024, 7) + group_array[1] + group_array[0];
	}
	else if (size <= 64 * 1024)
	{ // [8*1024+1,64*1024] 1024B -->10位
		return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
			+ group_array[0];
	}
	else if (size <= 256 * 1024)
	{ // [64*1024+1,256*1024] 8 * 1024B  -->13位
		return _Index(size - 64 * 1024, 13) + group_array[3] +
			group_array[2] + group_array[1] + group_array[0];
	}
	else
	{
		assert(false);
	}
	return -1;
}

    //块页匹配算法
    //size表示一块的大小
    static size_t NumMovePage(size_t size)
    {
        //当cc中没有span为tc提供小块空间时，cc就需要向pc申请一块span，此时需要根据一块空间的大小来匹配出一个维护页空间较为合适的span，
        //以保证span为size后尽量不浪费或不足够还再频繁申请相同大小的span

        //NumMoveSize是算出tc向cc申请size大小的块时的单次最大申请块数
        size_t num = NumMoveSize(size);

        //num * size 就是单次申请最大空间大小
        size_t npage = num * size;

        //PAGE_SHIFT表示一页要占用多少位，比如一页8KB就是13位，这里右移其实就是除以页大小，算出来就是单次申请最大空间有多少页
        npage >>= PAGE_SHIFT;

        /*
            如果算出来是0，就直接给1页，比如说size为8B时，num就是512，npage算出来就是4KB，
            那如果一页8KB，算出来直接为0了，意思就是半页的空间都够8B的单次申请的最大空间了，但是二进制中没有0.5，所以只能给1页
        */
        if(npage == 0)
            npage = 1;

        return npage;
    }



};








#endif