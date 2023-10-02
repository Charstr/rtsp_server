#ifndef _EVENT_SCHEDULER_H_
#define _EVENT_SCHEDULER_H_
#include <vector>
#include <queue>

#include "poller/PollPoller.h"
#include "Timer.h"
#include "threadPool/Mutex.h"

class EventScheduler
{

/*
EventScheduler用于添加、更新和删除不同类型的事件，包括触发事件和定时器事件。它还有一个循环函数用于处理事件，并且可以在本地线程中执行回调函数。

EventScheduler是一个事件调度器，用于管理触发事件和定时事件，以及处理I/ O事件。使用了不同的底层事件轮询机制（POLLER_SELECT、POLLER_POLL、POLLER_EPOLL），并提供了接口来添加和删除事件，以及运行事件循环。此外，它还支持唤醒事件调度器以处理其他事件。在事件处理的过程中，使用了TimerManager来管理定时事件，并使用IOEvent来处理唤醒事件。

在事件处理的过程中，EventScheduler会不断检查是否有触发事件需要处理，然后使用底层的事件轮询机制来处理I/O事件。同时，也可以通过TimerManager添加定时事件，定时事件的处理会在事件循环中定期触发。最后，在事件调度器的线程中，可以使用runInLocalThread来添加需要在事件循环线程中执行的回调函数，这些回调函数会在事件循环中被调用。
*/
public:
    typedef void (*Callback)(void*);

    enum PollerType
    {
        POLLER_SELECT,
        POLLER_POLL,
        POLLER_EPOLL
    };

    // 单例模式, 创建一个新的事件调度器实例
    static EventScheduler* createNew(PollerType type);

    EventScheduler(PollerType type, int fd);
    virtual ~EventScheduler();

    // 添加触发事件
    bool addTriggerEvent(TriggerEvent* event);
    // 添加定时事件，延迟一定时间后执行
    Timer::TimerId addTimedEventRunAfater(TimerEvent* event, Timer::TimeInterval delay);
    // 添加定时事件，在指定的时间点执行
    Timer::TimerId addTimedEventRunAt(TimerEvent* event, Timer::Timestamp when);
    // 添加定时事件，定期执行
    Timer::TimerId addTimedEventRunEvery(TimerEvent* event, Timer::TimeInterval interval);
    // 移除定时事件
    bool removeTimedEvent(Timer::TimerId timerId);

    // 添加I/O事件
    bool addIOEvent(IOEvent* event);
    // 更新I/O事件
    bool updateIOEvent(IOEvent* event);
    // 移除I/O事件
    bool removeIOEvent(IOEvent* event);


    // 事件循环，处理触发事件、I/O事件和定时事件
    // 用于处理事件，并且可以在本地线程中执行回调函数。
    void loop();
    // 唤醒事件调度器，用于处理其他事件
    void wakeup();
    // 在本地线程中运行回调函数
    void runInLocalThread(Callback callBack, void* arg);


    // 处理其他事件，如本地线程中添加的回调函数
    void handleOtherEvent();

private:

    // 处理触发事件
    void handleTriggerEvents();
    // 处理读回调
    static void handleReadCallback(void*);
    // 处理读事件
    void handleRead();

private:
    // reactor模式
    bool mQuit;
    Poller* mPoller;// 基于epull,监听多个文件描述符上的IO事件，并将就绪的事件通知给相应的事件处理器。
    TimerManager* mTimerManager; //定时器管理器，负责管理定时事件的触发和处理。
    std::vector<TriggerEvent*> mTriggerEvents;
    int mWakeupFd;//来唤醒阻塞在事件循环中的线程的文件描述符
    IOEvent* mWakeIOEvent; //监控和处理各种文件描述符上的事件，包括读、写、定时器事件等。

    std::queue<std::pair<Callback, void*> > mCallBackQueue;
    Mutex* mMutex;
};

#endif //_EVENT_SCHEDULER_H_