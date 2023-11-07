#ifndef _USAGEENVIRONMENT_H_
#define _USAGEENVIRONMENT_H_
#include "EventScheduler.h"
#include "threadPool/ThreadPool.h"

// 提供了处理事件调度和线程池的功能。
class UsageEnvironment {
public:
	// 静态工厂方法,创建一个新的UsageEnvironment对象，接受两个参数：EventScheduler*和ThreadPool*，分别表示事件调度器和线程池。
	// 该方法会分配内存并返回一个指向UsageEnvironment对象的指针。工厂模式允许创建对象并隐藏其构造细节。
	static UsageEnvironment *createNew(EventScheduler *scheduler, ThreadPool *threadPool);
	// 构造函数，接受一个EventScheduler和一个ThreadPool作为参数
	// 析构函数通常用于释放对象拥有的资源.
	// 没有释放mScheduler和mThreadPool指针，因为这些指针的生命周期由外部管理，对象只是持有对其他对象的引用
	UsageEnvironment(EventScheduler *scheduler, ThreadPool *threadPool);
	~UsageEnvironment();
	// 获取EventScheduler对象的指针
	EventScheduler *scheduler();
	// 获取ThreadPool对象的指针
	ThreadPool *threadPool();

private:
	EventScheduler *mScheduler; // 保存EventScheduler的指针
	ThreadPool *mThreadPool;	// 保存ThreadPool的指针
};

#endif //_USAGEENVIRONMENT_H_