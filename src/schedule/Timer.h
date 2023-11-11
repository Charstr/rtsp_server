#ifndef _TIMER_H_
#define _TIMER_H_
#include <map>
#include <stdint.h>

#include "Event.h"
#include "poller/PollPoller.h"

/*
1.
Timer和TimerManager类实现了一个简单的定时器管理系统，用于处理定时事件。Timer类表示单个定时器，TimerManager类负责管理多个定时器。
2.
每个Timer对象都关联了一个TimerEvent，当定时器触发时，相关的事件回调函数将被执行。这允许用户在定时器触发时执行自定义操作。
3.
定时器触发通过Linux提供的timerfd实现，使用timerFdSetTime函数来设置定时器的触发时间和时间间隔。一旦定时器超时，将调用handleTimerEvent函数来处理定时器事件。
4.
TimerManager类通过TimerEvent的指针来允许不同类型的定时事件。这允许用户创建多个不同类型的定时器事件。
*/

class Timer // 单个定时器
{
public:
	typedef uint32_t TimerId; // 定时器ID
	typedef int64_t Timestamp; // 毫秒级时间戳
	typedef uint32_t TimeInterval; // 毫秒级时间间隔

	~Timer();
	// 获取当前时间戳（毫秒级）
	static Timestamp getCurTime();

private:
	friend class TimerManager;
	Timer(TimerEvent *event, Timestamp timestamp, TimeInterval timeInterval);
	void handleEvent(); // 处理超时回调定时器事件

private:
	// 定时器事件，handleEvent时候执行的是设置的mTimerEvent的回调处理函数mTimeoutCallback
	TimerEvent *mTimerEvent;
	Timestamp mTimestamp; // 定时器触发时间戳
	TimeInterval mTimeInterval; // 定时器触发时间间隔
	bool mRepeat; // 定时器是否重复触发
};

// 管理多个定时器
class TimerManager {
public:
	// 创建TimerManager实例，传入一个Poller实例用于事件管理
	static TimerManager *createNew(Poller *poller);

	TimerManager(int timerFd, Poller *poller);
	~TimerManager();
	// 添加一个定时器，返回定时器ID
	Timer::TimerId addTimer(TimerEvent *event, Timer::Timestamp timestamp,
							Timer::TimeInterval timeInterval);
	// 移除指定ID的定时器
	bool removeTimer(Timer::TimerId timerId);

private:
	void modifyTimeout(); // 修改定时器超时时间
	static void handleRead(void *); // 定时器事件的读回调函数，处理多个定时器事件
	void handleTimerEvent(); // 处理定时器事件

private:
	Poller *mPoller; // 事件管理器
	int mTimerFd; // 定时器文件描述符
	// map容器存储定时器的TimerId和Timer对象之间的映射关系
	std::map<Timer::TimerId, Timer> mTimers; // 定时器事件

	typedef std::pair<Timer::Timestamp, Timer::TimerId> TimerIndex;
	// 定时器事件队列,以按时间顺序管理定时器。根据最早触发的定时器来调整timerfd的触发时间。
	std::multimap<TimerIndex, Timer> mEvents;

	uint32_t mLastTimerId; // 最后一个定时器的ID,当前使用的最大的TimerId
	IOEvent *mTimerIOEvent; // 定时器的IOEvent对象
};

#endif //_TIMER_H_