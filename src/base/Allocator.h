#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_
#include <cstddef>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mutex>

// 内存分配的类，考虑了64位系统8字节的内存对齐
class Allocator {
public:
	enum {
		ALIGN = 8 // 为什么是8？64位系统中内存对齐的大小是8字节
	}; // 内存对齐边界
	enum {
		MAX_BYTES = 128
	}; // 最大分配的内存块
	enum {
		NFREELISTS = MAX_BYTES / ALIGN
	}; // 自由链表的数量

	union Obj { // 内存块
		Obj *next;
		char data[1];
	};

	static void *allocate(uint32_t size); // 分配内存

	static void deallocate(void *p, uint32_t size); // 释放内存

private:
	// 默认构造初始化申请的堆内存大小为0
	Allocator() : mStartFree(NULL), mEndFree(NULL), mHeapSize(0) {
		memset(mFreeList, 0, sizeof(mFreeList));
	};

	~Allocator() {
		if (mAllocator) {
			delete mAllocator;
			mAllocator = nullptr;
		}
	};

	// 单例模式
	static Allocator *getInstance(); // 获取单例

	void *alloc(uint32_t size); // 内部分配接口
	void dealloc(void *p, uint32_t size); // 内部释放接口

	// 获取指定字节数在自由链表的下标
	// 这里内存对齐使用的是8字节的，所以移位3
	uint32_t freelistIndex(uint32_t bytes) {
		// return (((bytes) + ALIGN - 1) / ALIGN - 1);
		return (((bytes) + ALIGN - 1) >> 3) - 1;
	}

	// 内存对齐，向上取整
	uint32_t roundup(uint32_t bytes) {
		return (((bytes) + ALIGN - 1) & ~(ALIGN - 1));
	}

	void *refill(uint32_t bytes); // 重新填充内存
	char *chunkAlloc(uint32_t size, int &nobjs); // 分配大内存块

private:
	static Allocator *mAllocator; // 单例实例
	std::mutex m_mutex;

	// 维护缓存块，管理剩余没分配的起始地址
	char *mStartFree;
	char *mEndFree;
	uint32_t mHeapSize;

	// 使用一个链表数据结构来管理空闲内存块，并根据请求的大小选择适当的内存块来分配内存。如果请求的内存大小超过了最大分配内存大小，则直接使用malloc和free函数进行分配和释放。
	Obj *mFreeList[NFREELISTS]; // 自由链表数组
};

#endif //_ALLOCATOR_H_