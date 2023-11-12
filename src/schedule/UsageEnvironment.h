#ifndef _USAGEENVIRONMENT_H_
#define _USAGEENVIRONMENT_H_
#include "EventScheduler.h"
#include "threadPool/ThreadPool.h"

// 提供了处理事件调度和线程池的功能。
class UsageEnvironment {
public:
	static std::shared_ptr<UsageEnvironment>
	createNew(EventScheduler *scheduler, ThreadPool *threadPool);

	UsageEnvironment(EventScheduler *scheduler, ThreadPool *threadPool);
	~UsageEnvironment();
	// 获取EventScheduler对象的指针
	EventScheduler *scheduler();
	// 获取ThreadPool对象的指针
	ThreadPool *threadPool();

private:
	EventScheduler *mScheduler; // 保存EventScheduler的指针
	ThreadPool *mThreadPool; // 保存ThreadPool的指针
};

#endif //_USAGEENVIRONMENT_H_