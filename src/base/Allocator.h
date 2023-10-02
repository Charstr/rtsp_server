#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "schedule/threadPool/Mutex.h"


/*
Allocator实现了一个内存池的分配器,
1. 使用单例模式,保证全局只有一个Allocator实例
2. 使用预分配的内存池来管理小块内存,避免频繁调用malloc/free
3. 使用自由链表(free list)管理内存块,根据大小分类管理不同的链表
4. 使用缓存内存块来避免频繁向系统申请释放内存
5. 加锁保证多线程安全
6. 节省内存碎片,提高内存利用率


*/
class Allocator
{
public:
    enum {ALIGN = 8};//内存对齐边界
    enum {MAX_BYTES = 128};//最大分配的内存块
    enum {NFREELISTS = MAX_BYTES / ALIGN};//自由链表的数量

    union Obj {//内存块
        union Obj* next;
        char data[1];
    };

    static void* allocate(uint32_t size);//分配内存

    static void deallocate(void* p, uint32_t size);//释放内存

private:
    Allocator() : mStartFree(NULL), mEndFree(NULL), mHeapSize(0)
    {
        mMutex = new Mutex;//互斥锁
        memset(mFreeList, 0, sizeof(mFreeList));
    };

    ~Allocator() {

    };
    // 单例模式
    static Allocator* getInstance();//获取单例

    void* alloc(uint32_t size);//内部分配接口
    void dealloc(void* p, uint32_t size);//内部释放接口

    /* 获取指定字节数在自由链表的下标 */
    uint32_t freelistIndex(uint32_t bytes) {
        return (((bytes) + ALIGN-1) / ALIGN - 1);
    }

    /* 字节对齐 */
    uint32_t roundup(uint32_t bytes) {
        return (((bytes) + ALIGN-1) & ~(ALIGN - 1));
    }

    void *refill(uint32_t bytes);//重新填充内存
    char* chunkAlloc(uint32_t size, int& nobjs);//分配大内存块

private:
    static Allocator* mAllocator; //单例实例

    Mutex* mMutex;

    /* 维护缓存块 */
    char* mStartFree;
    char* mEndFree;
    uint32_t mHeapSize;
    // 使用一个链表数据结构来管理空闲内存块，并根据请求的大小选择适当的内存块来分配内存。如果请求的内存大小超过了最大分配内存大小，则直接使用malloc和free函数进行分配和释放。
    Obj* mFreeList[NFREELISTS]; //自由链表数组

};

#endif //_ALLOCATOR_H_