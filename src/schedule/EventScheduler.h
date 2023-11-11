#ifndef _EVENT_SCHEDULER_H_
#define _EVENT_SCHEDULER_H_
#include <mutex>
#include <queue>
#include <vector>

#include "Timer.h"
#include "poller/PollPoller.h"

/*

EventScheduler主要包含了Events和Poller(epoll)两大模块

用于添加、更新和删除不同类型的事件，包括触发事件和定时器事件。它还有一个循环函数用于处理事件，并且可以在本地线程中执行回调函数。

EventScheduler是一个事件调度器，用于管理触发事件和定时事件，以及处理I/
O事件。使用了不同的底层事件轮询机制（POLLER_SELECT、POLLER_POLL、POLLER_EPOLL），并提供了接口来添加和删除事件，以及运行事件循环。此外，它还支持唤醒事件调度器以处理其他事件。在事件处理的过程中，使用了TimerManager来管理定时事件，并使用IOEvent来处理唤醒事件。

在事件处理的过程中，EventScheduler会不断检查是否有触发事件需要处理，然后使用底层的事件轮询机制来处理I/O事件。同时，也可以通过TimerManager添加定时事件，定时事件的处理会在事件循环中定期触发。最后，在事件调度器的线程中，可以使用runInLocalThread来添加需要在事件循环线程中执行的回调函数，这些回调函数会在事件循环中被调用。
*/

class EventScheduler {
public:
	using Callback = std::function<void(void *)>;

	enum PollerType {
		POLLER_SELECT,
		POLLER_POLL,
		POLLER_EPOLL
	};

	static std::shared_ptr<EventScheduler> createNew(PollerType type);

	// // 工厂模式
	// static EventScheduler *createNew(PollerType type);
	// 构造函数
	EventScheduler(PollerType type, int fd);
	virtual ~EventScheduler();

	// 添加触发事件
	bool addTriggerEvent(std::shared_ptr<TriggerEvent> event);

	// 添加定时事件，延迟一定时间后执行
	Timer::TimerId
	addTimedEventRunAfater(std::shared_ptr<TimerEvent> event, Timer::TimeInterval delay);
	// 添加定时事件，在指定的时间点执行
	Timer::TimerId addTimedEventRunAt(std::shared_ptr<TimerEvent> event, Timer::Timestamp when);
	// 添加定时事件，定期执行
	Timer::TimerId
	addTimedEventRunEvery(std::shared_ptr<TimerEvent> event, Timer::TimeInterval interval);
	// 移除定时事件
	bool removeTimedEvent(Timer::TimerId timerId);

	// 添加I/O事件，实际上是直接调用poller的方法,epoll_ctl
	bool addIOEvent(std::shared_ptr<IOEvent> event);
	// 更新I/O事件
	bool updateIOEvent(std::shared_ptr<IOEvent> event);
	// 移除I/O事件
	bool removeIOEvent(std::shared_ptr<IOEvent> event);

	// 开启事件循环，处理触发事件、I/O事件和定时事件，并且可以在本地线程中执行回调函数。
	void loop();

	// 唤醒EventScheduler事件调度器所在的线程
	void wakeup();

	// 设置在本地线程中运行回调函数
	void runInLocalThread(Callback callBack, void *arg);

	// 处理其他事件，如本地线程中添加的回调函数
	void handleOtherEvent();

private:
	// 处理触发事件
	void handleTriggerEvents();

	// 被mWakeIOEvent的read事件回调函数调用,唤醒loop所在线程
	static void handleReadCallback(void *);
	// 被mWakeIOEvent的read事件回调函数调用,唤醒loop所在线程
	void handleRead();

private:
	bool mQuit; // 标志是否退出loop
	// 基于epoll,监听多个文件描述符上的IO事件，并将就绪的事件通知给相应的事件处理器。
	Poller *mPoller;

	TimerManager *mTimerManager; // 定时器管理器，负责管理定时事件的触发和处理。

	std::vector<std::shared_ptr<TriggerEvent>> mTriggerEvents; // 存储触发事件的对象

	int mWakeupFd; // 唤醒loop所在的线程的文件描述符
	std::shared_ptr<IOEvent> mWakeIOEvent; // 唤醒后上边的描述符对应的事件

	std::queue<std::pair<Callback, void *>> mCallBackQueue; // 存储loop需要执行的回调操作
	std::mutex m_mutex;
};

#endif //_EVENT_SCHEDULER_H_