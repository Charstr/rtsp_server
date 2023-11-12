#include <memory>
#include <stdio.h>

#include "UsageEnvironment.h"
#include "base/New.h"

std::shared_ptr<UsageEnvironment>
UsageEnvironment::createNew(EventScheduler *scheduler, ThreadPool *threadPool) {
	if (!scheduler)
		return nullptr;
	return std::make_shared<UsageEnvironment>(scheduler, threadPool);
}
// 构造函数，初始化成员变量
UsageEnvironment::UsageEnvironment(EventScheduler *scheduler, ThreadPool *threadPool)
	: mScheduler(scheduler), mThreadPool(threadPool) {
	// 在构造函数中接受EventScheduler和ThreadPool作为参数，并将它们保存在成员变量中
}

UsageEnvironment::~UsageEnvironment() {
	// 注意：在析构函数中没有释放mScheduler和mThreadPool指针的责任，因为这些指针的生命周期由外部管理
}

EventScheduler *UsageEnvironment::scheduler() {
	return mScheduler;
}

ThreadPool *UsageEnvironment::threadPool() {
	return mThreadPool;
}