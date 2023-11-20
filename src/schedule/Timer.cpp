#include <cstdio>
#include <sys/timerfd.h>

#include "Timer.h"
#include "base/Logging.h"
#include "base/New.h"

static int timerFdCreate(int clockid, int flags) {
	return timerfd_create(clockid, flags);
}

// 设置超时时间，也就是下次触发的时间
static bool timerFdSetTime(int fd, Timer::Timestamp when, Timer::TimeInterval period) {
	struct itimerspec newVal;

	newVal.it_value.tv_sec = when / 1000; // ms转换为s
	newVal.it_value.tv_nsec = when % 1000 * 1000 * 1000; // ms->ns
	newVal.it_interval.tv_sec = period / 1000; // ms转换为s
	newVal.it_interval.tv_nsec = period % 1000 * 1000 * 1000; // ms转换为ns
	// 系统调用设置定时器的超时时间和间隔时间。
	if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &newVal, NULL) < 0)
		return false;

	return true;
}

Timer::Timer(TimerEvent *event, Timestamp timestamp, TimeInterval timeInterval)
	: mTimerEvent(event), mTimestamp(timestamp), mTimeInterval(timeInterval) {
	mRepeat = timeInterval > 0 ? true : false;
}

Timer::~Timer() {}

// 获取当前时间的毫秒表示
Timer::Timestamp Timer::getCurTime() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	return (now.tv_sec * 1000 + now.tv_nsec / 1000000);
}

void Timer::handleEvent() {
	if (!mTimerEvent)
		return;

	mTimerEvent->handleEvent(); // 调用TimerEvent对象的事件处理函数
}

TimerManager *TimerManager::createNew(Poller *poller) {
	if (!poller)
		return nullptr;

	/*

	1. CLOCK_MONOTONIC使用单调时钟（不受系统时间调整影响的时钟）来度量时间。TFD_NONBLOCK
	表示非阻塞模式（即read() 操作不会阻塞），TFD_CLOEXEC 表示在执行exec()函数时关闭该文件描述符。
	2.
	定时器文件描述符作为一个事件交给Reactor。定时器队列采用管理超时时间，消费者有一个定时器，间隔一定时间就会向生产者取数据，并将数据RTP打包再传输
	*/

	int timerFd = timerFdCreate(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (timerFd < 0) {
		LOG_ERROR("failed to create timer fd\n");
		return nullptr;
	}

	// return new TimerManager(timerFd, poller);
	return New<TimerManager>::allocate(timerFd, poller);
}

TimerManager::TimerManager(int timerFd, Poller *poller)
	: mTimerFd(timerFd), // 定时器文件描述符
	  mPoller(poller), mLastTimerId(0) {

	// 根据定时器文件描述符fd创建定时器IO事件，多路复用监听mTimerFd，
	// 描述符有事件了，对应的是定时器IO事件，会处理多个不同的定时器事件，分别调用各自的处理函数
	// 这里指的是定时发送rtp包
	mTimerIOEvent = IOEvent::createNew(mTimerFd, this);
	// 定时器IO事件的回调函数
	mTimerIOEvent->setReadCallback(TimerManager::handleRead);
	mTimerIOEvent->enableReadHandling();
	// 这里是文件描述符mTimerFd的超时时间修正

	modifyTimeout(); // 修正超时时间

	// 将定时器IO事件添加到事件调度中
	mPoller->addIOEvent(mTimerIOEvent);
}

/*--------------处理定时事件-------------------*/

// 定时器IO事件mTimerIOEvent触发
void TimerManager::handleRead(void *arg) {
	if (!arg)
		return;
	TimerManager *timerManager = (TimerManager *)arg;
	timerManager->handleTimerEvent();
}

void TimerManager::handleTimerEvent() {

	if (!mTimers.empty()) { // 有定时器事件要处理

		int64_t timePoint = Timer::getCurTime(); // 当前时间戳以毫秒为单位

		// TimerId定时器ID，Timer单个定时器，Timestamp时间戳
		// map<TimerId, Timer> mTimers; 定时器ID和定时器映射
		// multimap<pair<Timestamp, TimerId>, Timer> mEvents; 与定时器事件对应的事件
		// map和multimap都会进行自动排序，mTimers会按照TimerId排序，mEvents先按照Timestamp再TimerId

		// 有定时事件要处理，且定时器事件的时间戳小于等于当前时间点，就进行处理
		while (!mTimers.empty() && mEvents.begin()->first.first <= timePoint) {

			Timer::TimerId timerId = mEvents.begin()->first.second; // 定时器ID
			Timer timer = mEvents.begin()->second; // 定时器
			// 通过设置的定时事件回调函数处理事件
			timer.handleEvent();
			mEvents.erase(mEvents.begin()); // 执行完之后从事件队列删除该事件

			// 如果定时器事件需要重复执行，则计算下一次触发的时间戳，将该事件重新加入到事件队列mEvents中以便下次触发
			if (timer.mRepeat) {
				timer.mTimestamp = timePoint + timer.mTimeInterval; // 下次执行的时间戳
				// 按照时间戳排序的事件
				mEvents.insert(std::make_pair(TimerIndex(timer.mTimestamp, timerId), timer));

			} else
				mTimers.erase(timerId); // 不需要重复就删除对应的定时器
		}
	}

	// 到这里，要么mTimers为空，要么是事件发生的时间戳都比当前的时间大，可以执行的定时器事件都执行完了
	// 接下来就要修改 mTimerFd
	// 的超时时间为下一个即将触发的事件的时间戳，当计时器到达这个时间戳的时候，
	// mTimerFd相关的IO事件mTimerIOEvent就要被触发
	modifyTimeout();
}

/*---------------管理定时事件----------------------*/

Timer::TimerId TimerManager::addTimer(
	TimerEvent *event, Timer::Timestamp timestamp, Timer::TimeInterval timeInterval) {
	// event是mTimerEvent，定时触发的事件，
	Timer timer(event, timestamp, timeInterval); // 创建定时器

	++mLastTimerId; // 新添加的定时器索引

	// 存储要触发的定时器，实际上不一定是按照时间顺序，后边可能有要重复的
	mTimers.insert(std::make_pair(mLastTimerId, timer));

	// 添加定时器事件到事件队列
	mEvents.insert(std::make_pair(TimerIndex(timestamp, mLastTimerId), timer));

	modifyTimeout();

	return mLastTimerId;
}

bool TimerManager::removeTimer(Timer::TimerId timerId) {
	std::map<Timer::TimerId, Timer>::iterator it = mTimers.find(timerId);
	if (it != mTimers.end()) {
		Timer::Timestamp timestamp = it->second.mTimestamp;
		Timer::TimerId timerId = it->first;
		mEvents.erase(TimerIndex(timestamp, timerId));
		mTimers.erase(timerId);
	}

	modifyTimeout();

	return true;
}

// 修改 mTimerFd
// 的超时时间为下一个最早触发的事件的时间戳，当计时器到达这个时间戳的时候，mTimerFd相关的IO事件mTimerIOEvent就要被触发，调用相应的回调函数执行定时器任务。
// 很多地方函数调用后都需要修正触发事件
void TimerManager::modifyTimeout() {

	// 定时器队列采用multimap管理超时时间，会自动根据时间戳排序
	std::multimap<TimerIndex, Timer>::iterator it = mEvents.begin();
	if (it != mEvents.end()) {
		// 事件队列中还有待触发的事件，取出最早触发的事件，将定时器文件描述符的超时时间设置为该事件的时间戳和间隔时间，这样，定时器将在该时间点触发，并执行相应的操作。
		Timer timer = it->second; // 定时器
		timerFdSetTime(mTimerFd, timer.mTimestamp, timer.mTimeInterval);
	} else
		timerFdSetTime(mTimerFd, 0, 0); // 定时器队列为空，取消定时器，将超时时间设置为0
}

TimerManager::~TimerManager() {
	mPoller->removeIOEvent(mTimerIOEvent);
	// 释放定时器IO事件资源
	Delete::release(mTimerIOEvent);
}