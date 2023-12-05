#ifndef _EVENT_SCHEDULER_H_
#define _EVENT_SCHEDULER_H_

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "Timer.h"
#include "poller/PollPoller.h"

class EventScheduler {
public:
	using Callback = std::function<void(void *)>;
	// typedef void (*Callback)(void *);

	enum PollerType {
		POLLER_SELECT,
		POLLER_POLL,
		POLLER_EPOLL
	};

	static std::shared_ptr<EventScheduler> createNew(PollerType type);
	EventScheduler(PollerType type, int fd);
	virtual ~EventScheduler();

	// 添加触发事件
	bool addTriggerEvent(TriggerEvent *event);

	// 添加定时事件，延迟一定时间后执行
	Timer::TimerId addTimedEventRunAfater(TimerEvent *event, Timer::TimeInterval delay);
	// 添加定时事件，在指定的时间点执行
	Timer::TimerId addTimedEventRunAt(TimerEvent *event, Timer::Timestamp when);
	// 添加定时事件，定期执行
	Timer::TimerId addTimedEventRunEvery(TimerEvent *event, Timer::TimeInterval interval);
	// 移除定时事件
	bool removeTimedEvent(Timer::TimerId timerId);

	// 添加I/O事件，实际上是直接调用poller的方法,epoll_ctl
	bool addIOEvent(IOEvent *event);
	// 更新I/O事件
	bool updateIOEvent(IOEvent *event);
	// 移除I/O事件
	bool removeIOEvent(IOEvent *event);

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

	std::vector<TriggerEvent *> mTriggerEvents; // 存储触发事件的对象

	int mWakeupFd; // 唤醒loop所在的线程的文件描述符
	IOEvent *mWakeIOEvent; // 唤醒后上边的描述符对应的事件

	std::queue<std::pair<Callback, void *>> mCallBackQueue; // 存储loop需要执行的回调操作
	std::mutex m_mutex;
};

#endif //_EVENT_SCHEDULER_H_