#include <iostream>
#include <mutex>
#include <stdlib.h>

#include "Allocator.h"

Allocator *Allocator::mAllocator = NULL;

void *Allocator::allocate(uint32_t size) {
	return getInstance()->alloc(size); // 调用单例的alloc
}

void Allocator::deallocate(void *p, uint32_t size) {
	getInstance()->dealloc(p, size); // 调用单例的dealloc
}

Allocator *Allocator::getInstance() {
	if (!mAllocator)
		mAllocator = new Allocator(); // 懒汉式单例

	return mAllocator;
}

void *Allocator::alloc(uint32_t size) {
	Obj *result;
	uint32_t index;

	std::lock_guard<std::mutex> lock(m_mutex);

	/* 如果分配内存大于 MAX_BYTES，那么就直接通过 malloc 分配 */
	if (size > MAX_BYTES)
		return malloc(size);

	index = freelistIndex(size); // 获取索引
	result = mFreeList[index];

	// 最开始的时候，是没有分配内存会走到这里，同时没有找到合适的也会重新分配内存
	// 分配时候考虑内存对齐
	if (!result) {
		void *r = refill(roundup(size));
		return r;
	}

	/* 找到了就从链表中删除内存块 */
	mFreeList[index] = result->next;

	return result;
}

void Allocator::dealloc(void *p, uint32_t size) {
	Obj *obj = (Obj *)p;
	uint32_t index;

	std::lock_guard<std::mutex> lock(m_mutex);

	/* 如果释放内存大于 MAX_BYTES，那么就直接通过 free 释放 */
	if (size > MAX_BYTES) {
		free(p);
		return;
	}

	index = freelistIndex(size); // 获取该大小在freelist的下标

	/* 将内存块添加进链表中 */
	obj->next = mFreeList[index];
	mFreeList[index] = obj;
}

/* 重新分配内存 */
void *Allocator::refill(uint32_t bytes) {
	int nobjs = 20;
	char *chunk = chunkAlloc(bytes, nobjs);
	Obj *result;
	Obj *currentObj;
	Obj *nextObj;
	int i;
	uint32_t index;

	if (1 == nobjs)
		return chunk;

	result = (Obj *)chunk;
	index = freelistIndex(bytes);
	mFreeList[index] = nextObj = (Obj *)(chunk + bytes);

	// 将剩余内存连成链表
	for (i = 1;; ++i) {
		currentObj = nextObj;
		nextObj = (Obj *)((char *)nextObj + bytes);

		// 最后一个节点
		if (nobjs - 1 == i) {
			currentObj->next = 0;
			break;
		} else {
			currentObj->next = nextObj;
		}
	}

	return result;
}

char *Allocator::chunkAlloc(uint32_t size, int &nobjs) {
	// 对于每一片内存，都预想的是先分配20块
	char *result; // 给这块分配的内存
	uint32_t totalBytes = size * nobjs; // 总字节数申请这么多个
	uint32_t bytesLeft = mEndFree - mStartFree; // 缓存块剩余空间大小

	// 如果缓存块空间充足，则直接从缓存块中获取内存20块这个内存
	if (bytesLeft > totalBytes) {
		result = mStartFree; // 分配的起始
		mStartFree += totalBytes; // 剩余的起始
		return result;
	} else if (bytesLeft > size) {

		// 缓存块不能完全满足，分配少于20块的内存块
		nobjs = bytesLeft / size;
		totalBytes = size * nobjs;
		result = mStartFree;
		mStartFree += totalBytes;
		return result;
	} else {
		// 缓存块剩余空间不足以分配任何对象
		// 重新分配内存
		uint32_t bytesToGet = 2 * totalBytes + roundup(mHeapSize >> 4); // 至少两倍增长

		// 缓存块还有碎片，放入free list
		if (bytesLeft > 0) {
			uint32_t index = freelistIndex(bytesLeft);
			((Obj *)(mStartFree))->next = mFreeList[index];
			mFreeList[index] = (Obj *)mStartFree;
		}

		/* 重新申请内存 */
		mStartFree = (char *)malloc(bytesToGet);

		mHeapSize += bytesToGet;
		mEndFree = mStartFree + bytesToGet;

		/* 递归调用chunkAlloc，重新分配 */
		return chunkAlloc(size, nobjs);
	}
}