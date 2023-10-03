#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

#include "EventScheduler.h"
#include "poller/SelectPoller.h"
#include "poller/PollPoller.h"
#include "poller/EPollPoller.h"
#include "base/Logging.h"
#include "base/New.h"

//EventScheduler类是一个事件调度器，用于管理和触发各种事件。用不同的轮询器(Poller)来处理IO事件和定时事件。一些辅助函数和回调函数，用于处理触发事件和其他事件的逻辑。

static int createEventFd()
{
    int evtFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtFd < 0)
    {
        LOG_ERROR("failed to create event fd\n");
        return -1;
    }

    return evtFd;
}


// 工厂方法，创建一个新的事件调度器EventScheduler实例
EventScheduler* EventScheduler::createNew(PollerType type){
    if(type != POLLER_SELECT && type != POLLER_POLL && type != POLLER_EPOLL)
        return NULL;

    int evtFd = createEventFd();// mWakeupFd，每个EventLoop对象都有自己的eventfd
    if (evtFd < 0)
        return NULL;

    // 使用I/O多路复用来监听文件描述符的状态,当RTSP服务器需要同时处理多个描述符时，可以，并在有事件发生时进行相应的处理
    // 通过单例模式申请内存并强转类型然后在这块内存构造对象
    return New<EventScheduler>::allocate(type, evtFd);
}

// EventScheduler构造函数
EventScheduler::EventScheduler(PollerType type, int fd) :
    mQuit(false),
    mWakeupFd(fd)
{
    switch (type)
    {
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
    
    // mPoller 是事件轮询器，负责监听多个文件描述符上的事件，并将就绪的事件通知给相应的事件处理器

    // 定时器管理器，负责管理定时事件的触发和处理,维护了一个定时器队列，用于存储各种定时任务，如定时发送数据、定时任务执行等。当定时事件到达时，TimerManager 调用注册的回调函数，执行相应的操作。

    // 执行mPoller->addIOEvent(mTimerIOEvent);
    mTimerManager = TimerManager::createNew(mPoller);

    // 某个IO唤醒后的事件，要进行处理的事件
    mWakeIOEvent = IOEvent::createNew(mWakeupFd, this);

    // 设置mWakeupFd的事件类型和回调函数的函数指针，
    mWakeIOEvent->setReadCallback(EventScheduler::handleReadCallback);
    mWakeIOEvent->enableReadHandling(); // 设置事件类型

    // 把唤醒事件添加到调度管理器，对应于muduo的Channel::update实现
    // Poller这里通过多态，调用的是epoll的addIOEvent函数
    mPoller->addIOEvent(mWakeIOEvent);
    // 创建一个互斥锁
    mMutex = Mutex::createNew();
}

// 添加触发事件
bool EventScheduler::addTriggerEvent(TriggerEvent* event)
{
    mTriggerEvents.push_back(event);

    return true;
}

// 添加定时事件，在一定时间后执行
Timer::TimerId EventScheduler::addTimedEventRunAfater(TimerEvent* event, Timer::TimeInterval delay)
{
    Timer::Timestamp when = Timer::getCurTime();
    when += delay;
    
    return mTimerManager->addTimer(event, when, 0);
}
// 添加定时事件，指定执行时间点
Timer::TimerId EventScheduler::addTimedEventRunAt(TimerEvent* event, Timer::Timestamp when)
{
    return mTimerManager->addTimer(event, when, 0);
}

// 添加定时事件，定期执行
Timer::TimerId EventScheduler::addTimedEventRunEvery(TimerEvent* event, Timer::TimeInterval interval)
{
    Timer::Timestamp when = Timer::getCurTime();
    when += interval;

    return mTimerManager->addTimer(event, when, interval);
}

// 移除定时事件
bool EventScheduler::removeTimedEvent(Timer::TimerId timerId)
{
    return mTimerManager->removeTimer(timerId);
}

// 添加I/O事件
bool EventScheduler::addIOEvent(IOEvent* event)
{
    return mPoller->addIOEvent(event);
}

// 更新I/O事件
bool EventScheduler::updateIOEvent(IOEvent* event)
{
    return mPoller->updateIOEvent(event);
}

// 移除I/O事件
bool EventScheduler::removeIOEvent(IOEvent* event)
{
    return mPoller->removeIOEvent(event);
}

// 循环处理触发事件、处理IO事件和处理其他事件。
void EventScheduler::loop()
{
    while(mQuit != true)
    {
        // 处理触发事件
        this->handleTriggerEvents();
         // 处理IO事件
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
void EventScheduler::wakeup()
{
    uint64_t one = 1;
    ssize_t ret = ::write(mWakeupFd, &one, sizeof(one));
    if(ret!=sizeof(one)){
		LOG_ERROR("EventScheduler::wakeup() writes %d bytes instead of 8 \n",ret);   
    }
    
}

// 处理触发事件
void EventScheduler::handleTriggerEvents()
{
    if(!mTriggerEvents.empty())
    {
        for(std::vector<TriggerEvent*>::iterator it = mTriggerEvents.begin();
            it != mTriggerEvents.end(); ++it)
        {
            (*it)->handleEvent();
        }

        mTriggerEvents.clear();
    }
}
// 用于IO事件的回调函数，用于处理唤醒事件
void EventScheduler::handleReadCallback(void* arg)
{
    if(!arg) return;

    EventScheduler* scheduler = (EventScheduler*)arg;
    scheduler->handleRead();
}

// mWakeupFd的回调函数
void EventScheduler::handleRead()
{
    uint64_t one;
    // 读取所有的唤醒事件
    while(::read(mWakeupFd, &one, sizeof(one)) > 0);
}
// 在本地线程中运行回调函数
void EventScheduler::runInLocalThread(Callback callBack, void* arg)
{
    MutexLockGuard mutexLockGuard(mMutex);
    mCallBackQueue.push(std::make_pair(callBack, arg));
}
// 处理其他事件，如本地线程中添加的回调函数
void EventScheduler::handleOtherEvent()
{
    MutexLockGuard mutexLockGuard(mMutex);
    while(!mCallBackQueue.empty())
    {
        std::pair<Callback, void*> event = mCallBackQueue.front();
        event.first(event.second);
    }
}


// EventScheduler析构函数，清理资源
EventScheduler::~EventScheduler()
{
    mPoller->removeIOEvent(mWakeIOEvent);
    ::close(mWakeupFd);

    Delete::release(mWakeIOEvent);
    Delete::release(mTimerManager);
    Delete::release(mPoller);
    Delete::release(mMutex);
}
