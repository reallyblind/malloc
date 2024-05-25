#include "../include/ConcurrentAlloc.h"
#include <type_traits>

// 线程1执行方法
void Alloc1()
{// 两个线程调用ConncurrentAlloc测试能跑通不
	for (int i = 0; i < 5; ++i)
	{
		ConcurrentAlloc(6);
	}
}

// 线程2执行方法
void Alloc2()
{// 两个线程调用ConncurrentAlloc测试能跑通不
	for (int i = 0; i < 5; ++i)
	{
		ConcurrentAlloc(7);
	}
}



void AllocTest()
{
	std::thread t1(Alloc1);
	

	std::thread t2(Alloc2);
	t1.join();
	t2.join();
}

void ConcurrentAllocTest1()
{
	void* ptr1 = ConcurrentAlloc(5);
	void* ptr2 = ConcurrentAlloc(5);
	void* ptr3 = ConcurrentAlloc(5);
	void* ptr4 = ConcurrentAlloc(5);
	void* ptr5 = ConcurrentAlloc(5);

	std::cout<< ptr1 <<std::endl;
	std::cout<< ptr2 <<std::endl;
	std::cout<< ptr3 <<std::endl;
	std::cout<< ptr4 <<std::endl;
	std::cout<< ptr5 <<std::endl;
}

void ConcurrentAllocTest2()
{
	for(int i = 0; i < 1024; ++i)
	{
		void* ptr = ConcurrentAlloc(5);
		std::cout << ptr << std::endl;
	}

	void* ptr = ConcurrentAlloc(3);
	cout << "-----" << ptr << endl;
	
}

int main()
{
    //AllocTest();
	ConcurrentAllocTest1();
	
    return 0;

}