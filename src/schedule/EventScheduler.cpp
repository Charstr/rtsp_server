#include <sys/eventfd.h>
#include <unistd.h>
#include <stdint.h>

#include "EventScheduler.h"
#include "poller/SelectPoller.h"
#include "poller/PollPoller.h"
#include "poller/EPollPoller.h"
#include "base/Logging.h"
#include "base/New.h"

// 创建一个事件文件描述符
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
// 创建一个新的事件调度器EventScheduler实例
EventScheduler* EventScheduler::createNew(PollerType type){
    if(type != POLLER_SELECT && type != POLLER_POLL && type != POLLER_EPOLL)
        return NULL;

    int evtFd = createEventFd();// 事件的fd
    if (evtFd < 0)
        return NULL;

    //return new EventScheduler(type, evtFd);
    // 通过工厂方法创建一个EventScheduler实例
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
     
    // Poller* mPoller 是事件轮询器,在事件循环中被用来等待和检测事件的发生,负责监听多个文件描述符上的事件，并将就绪的事件通知给相应的事件处理器,使用不同的I/O多路复用技术（如select、poll、epoll）来实现事件监听，提高效率。
    // 定时器管理器，负责管理定时事件的触发和处理,维护了一个定时器队列，用于存储各种定时任务，如定时发送数据、定时任务执行等。当定时事件到达时，TimerManager 调用注册的回调函数，执行相应的操作。
    mTimerManager = TimerManager::createNew(mPoller);
    // IOEvent事件处理器，负责监控和处理各种文件描述符上的事件，包括读、写、定时器事件等。与不同的文件描述符（如套接字、定时器描述符等）关联，并注册回调函数，以便在事件发生时执行相应的操作。通常会包含一个状态机，用于处理不同的事件类型。
    // 唤醒事件fd
    mWakeIOEvent = IOEvent::createNew(mWakeupFd, this);
    mWakeIOEvent->setReadCallback(handleReadCallback);
    mWakeIOEvent->enableReadHandling();
    mPoller->addIOEvent(mWakeIOEvent); // 将IO事件添加到事件循环中
    // 创建一个互斥锁
    mMutex = Mutex::createNew();
}
// EventScheduler析构函数，清理资源
EventScheduler::~EventScheduler()
{
    mPoller->removeIOEvent(mWakeIOEvent);
    ::close(mWakeupFd);

    //delete mWakeIOEvent;
    //delete mTimerManager;
    //delete mPoller;
    Delete::release(mWakeIOEvent);
    Delete::release(mTimerManager);
    Delete::release(mPoller);
    Delete::release(mMutex);
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
// 唤醒事件调度器，用于处理其他事件
void EventScheduler::wakeup()
{
    uint64_t one = 1;
    int ret;
    ret = ::write(mWakeupFd, &one, sizeof(one));
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
    if(!arg)
        return;

    EventScheduler* scheduler = (EventScheduler*)arg;
    scheduler->handleRead();
}
// 处理唤醒事件
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