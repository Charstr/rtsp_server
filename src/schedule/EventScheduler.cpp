#include <cstdio>
#include <mutex>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "EventScheduler.h"
#include "base/Logging.h"
#include "base/New.h"
#include "poller/EPollPoller.h"
#include "poller/PollPoller.h"
#include "poller/SelectPoller.h"

static int createEventFd() {
	// 当一个进程调用 exec()
	// 启动一个新程序时，所有已打开的文件描述符都会被关闭，除非文件描述符被设置为 FD_CLOEXEC
	// 标志。这样可以避免在新程序中不小心操作这个文件描述符。 用于进程间的事件通知
	int evtFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (evtFd < 0) {
		LOG_ERROR("failed to create event fd\n");
		return -1;
	}
	return evtFd;
}

std::shared_ptr<EventScheduler> EventScheduler::createNew(PollerType type) {

	if (type != POLLER_SELECT && type != POLLER_POLL && type != POLLER_EPOLL)
		return nullptr;

	int evtFd = createEventFd(); // 用作mWakeupFd，每个EventLoop对象都有自己的eventfd
	if (evtFd < 0)
		return nullptr;
	return std::make_shared<EventScheduler>(type, evtFd);
}

EventScheduler::EventScheduler(PollerType type, int fd)
	: mQuit(false), mWakeupFd(fd) // 用于唤醒等待中的线程的文件描述符 evtfd
{

	// mPoller 负责监听多个文件描述符上的事件，并将就绪的事件通知给相应的事件处理器
	// 构造函数创建描述符mEPollFd
	switch (type) {
	case POLLER_SELECT:
		mPoller = SelectPoller::createNew();
		break;

	case POLLER_POLL:
		mPoller = PollPoller::createNew();
		break;

	case POLLER_EPOLL:
		mPoller = EPollPoller::createNew();
		break;

	default:
		_exit(-1);
		break;
	}

	// 执行mPoller->addIOEvent(mTimerIOEvent);
	mTimerManager = TimerManager::createNew(mPoller);

	// mWakeIOEvent用于监听 mWakeupFd 上的读事件
	mWakeIOEvent = IOEvent::createNew(mWakeupFd, this);

	// 读取 mWakeupFd 中的数据唤醒等待在EventScheduler上的线程
	mWakeIOEvent->setReadCallback(EventScheduler::handleReadCallback);
	mWakeIOEvent->enableReadHandling();

	mPoller->addIOEvent(mWakeIOEvent);
}

// 添加触发事件mTriggerEvent到
bool EventScheduler::addTriggerEvent(TriggerEvent *event) {
	mTriggerEvents.push_back(event);

	return true;
}

// 添加定时事件，在一定时间后执行
Timer::TimerId
EventScheduler::addTimedEventRunAfater(TimerEvent *event, Timer::TimeInterval delay) {
	Timer::Timestamp when = Timer::getCurTime();
	when += delay;

	return mTimerManager->addTimer(event, when, 0);
}

// 添加定时事件，指定执行时间点
Timer::TimerId EventScheduler::addTimedEventRunAt(TimerEvent *event, Timer::Timestamp when) {
	return mTimerManager->addTimer(event, when, 0);
}

// 添加定时事件，定期执行, 到when的时候触发，触发的间隔为interval
Timer::TimerId
EventScheduler::addTimedEventRunEvery(TimerEvent *event, Timer::TimeInterval interval) {
	Timer::Timestamp when = Timer::getCurTime();
	when += interval;
	// 设置定时器触发的间隔
	return mTimerManager->addTimer(event, when, interval);
}

// 移除定时事件
bool EventScheduler::removeTimedEvent(Timer::TimerId timerId) {
	return mTimerManager->removeTimer(timerId);
}

// 添加I/O事件
bool EventScheduler::addIOEvent(IOEvent *event) {
	return mPoller->addIOEvent(event);
}

// 更新I/O事件
bool EventScheduler::updateIOEvent(IOEvent *event) {
	return mPoller->updateIOEvent(event);
}

// 移除I/O事件
bool EventScheduler::removeIOEvent(IOEvent *event) {
	return mPoller->removeIOEvent(event);
}

// 工作线程
void EventScheduler::loop() {

	while (mQuit != true) {

		// 处理触发事件，调用的回调函数指针mTriggerCallback设置的是RtspServer::triggerCallback函数，遍历需要断开连接的mDisconnectionlist，根据映射关系从mConnections取出要断开的连接对应的RtspConnection，释放内存并从mConnections移除对应的连接描述符
		this->handleTriggerEvents();

		// 处理IO事件，epoll_wait把发生的事件赋值到事件数组中（vector<epoll_event>
		// mEPollEventList，并返回数目nums，然后遍历事件数组的前nums个，根据epoll返回的具体事件类型，把发生的事件添加到mEvents，然后遍历mEvents，分别调用各个IOEvent设置的回调函数进行处理。
		// 这里是epoll的handleEvent的入口，遍历IOEvent的数组
		mPoller->handleEvent();
		// 处理其他事件
		this->handleOtherEvent();
	}
}

/*
向想要唤醒的线程所绑定的EventScheduler对象的mWakeupFd随便写一个8字节数据

mWakeupFd已经注册到这个EventScheduler的事件监听器上,此时事件监听器监听到文件描述符的事件发生，epoll_Wait阻塞结束并返回,就相当于起了唤醒线程的作用

// EventScheduler既然阻塞在事件监听上,就通过mWakeupFd给EventScheduler对象一个事件,结束阻塞

*/
// 这个在哪用？
void EventScheduler::wakeup() {
	uint64_t one = 1;
	ssize_t ret = ::write(mWakeupFd, &one, sizeof(one));
	if (ret != sizeof(one)) {
		LOG_ERROR("EventScheduler::wakeup() writes %d bytes instead of 8 \n", ret);
	}
}

// 处理断开连接的触发事件
void EventScheduler::handleTriggerEvents() {
	if (!mTriggerEvents.empty()) {
		for (std::vector<TriggerEvent *>::iterator it = mTriggerEvents.begin();
			 it != mTriggerEvents.end(); ++it) {
			(*it)->handleEvent();
		}

		mTriggerEvents.clear();
	}
}

// 用于处理唤醒事件的回调函数
void EventScheduler::handleReadCallback(void *arg) {
	if (!arg)
		return;
	// printf("mWakeIOEvent回调\n");
	EventScheduler *scheduler = (EventScheduler *)arg;
	scheduler->handleRead();
}

// mWakeupFd的回调函数
void EventScheduler::handleRead() {

	uint64_t one;
	// 读取所有的唤醒事件
	while (::read(mWakeupFd, &one, sizeof(one)) > 0) {
	}
}

// 设置在本地线程处理的回调回调函数
void EventScheduler::runInLocalThread(Callback callBack, void *arg) {
	std::lock_guard<std::mutex> lguard(m_mutex);

	mCallBackQueue.push(std::make_pair(callBack, arg));
}

// 处理其他事件，如本地线程中添加的回调函数
void EventScheduler::handleOtherEvent() {
	std::lock_guard<std::mutex> lguard(m_mutex);
	while (!mCallBackQueue.empty()) {
		std::pair<Callback, void *> event = mCallBackQueue.front();
		event.first(event.second);
	}
}

// EventScheduler析构函数，清理资源
EventScheduler::~EventScheduler() {
	mPoller->removeIOEvent(mWakeIOEvent);
	::close(mWakeupFd);

	Delete::release(mWakeIOEvent);
	Delete::release(mTimerManager);
	Delete::release(mPoller);
}
