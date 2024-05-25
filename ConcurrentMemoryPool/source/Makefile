# 6-11行代码相似性很强，可以提取出一个公式模版
#      %.o:%.c
#	       $(CC) $^ $(CFLAGS)  -o $@
# 百分号相当于一个通配符

OBJS=UnitTest.o ThreadCache.o CentralCache.o PageCache.o
CC=g++
CFLAGS=-c -Wall -g -O0

mytool:$(OBJS)
	$(CC) -g $^ -o  mytool -lpthread 
%.o:%.cpp
	$(CC) $^ $(CFLAGS)  -o  $@ 

clean:
	$(RM) *.o mytool -r
