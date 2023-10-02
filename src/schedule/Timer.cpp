#include <sys/timerfd.h>

#include "Timer.h"
#include "base/Logging.h"
#include "base/New.h"

static int timerFdCreate(int clockid, int flags)
{
    return timerfd_create(clockid, flags);
}

static bool timerFdSetTime(int fd, Timer::Timestamp when, Timer::TimeInterval period)
{
    struct itimerspec newVal;

    newVal.it_value.tv_sec = when / 1000;  // ms转换为s
    newVal.it_value.tv_nsec = when % 1000 * 1000 * 1000; //ms->ns
    newVal.it_interval.tv_sec = period / 1000;// ms转换为s
    newVal.it_interval.tv_nsec = period % 1000 * 1000 * 1000; // ms转换为ns

    if (timerfd_settime(fd, TFD_TIMER_ABSTIME, &newVal, NULL) < 0)
        return false;

    return true;
}

Timer::Timer(TimerEvent* event, Timestamp timestamp, TimeInterval timeInterval) :
            mTimerEvent(event),
            mTimestamp(timestamp),
            mTimeInterval(timeInterval)
{
    if (timeInterval > 0)
        mRepeat = true;
    else
        mRepeat = false;
}

Timer::~Timer()
{

}
// 获取当前时间的毫秒表示
Timer::Timestamp Timer::getCurTime()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*1000 + now.tv_nsec/1000000);
}

void Timer::handleEvent()
{
    if(!mTimerEvent) return;

    mTimerEvent->handleEvent();// 调用TimerEvent对象的事件处理函数
}

TimerManager* TimerManager::createNew(Poller* poller)
{
    if(!poller)
        return NULL;

    // Linux提供的定时器timerfd_create，将定时器文件描述符作为一个事件交给Reactor
    // 定时器队列采用管理超时时间
    // 消费者有一个定时器，间隔一定时间就会向生产者取数据，并将数据RTP打包再传输
    // 时间的文件描述符
    int timerFd = timerFdCreate(CLOCK_MONOTONIC,
                                TFD_NONBLOCK | TFD_CLOEXEC);
    if(timerFd < 0)
    {
        LOG_ERROR("failed to create timer fd\n");
        return NULL;
    }

    //return new TimerManager(timerFd, poller);
    return New<TimerManager>::allocate(timerFd, poller);
}

TimerManager::TimerManager(int timerFd, Poller* poller) :
    mTimerFd(timerFd),
    mPoller(poller),
    mLastTimerId(0)
{   

    // 定时器IO文件描述
    mTimerIOEvent = IOEvent::createNew(mTimerFd, this);

    // 设置为回调并激活
    mTimerIOEvent->setReadCallback(TimerManager::handleRead);
    mTimerIOEvent->enableReadHandling();
    modifyTimeout();
    
    // 将定时器IO事件添加到事件循环中
    mPoller->addIOEvent(mTimerIOEvent);
}


void TimerManager::handleRead(void* arg)
{
    if(!arg) return;

    TimerManager* timerManager = (TimerManager*)arg;
    timerManager->handleTimerEvent();
}

// 发生定时事件
void TimerManager::handleTimerEvent()
{
    if(!mTimers.empty())
    {
        int64_t timePoint = Timer::getCurTime();

        while(!mTimers.empty() && mEvents.begin()->first.first <= timePoint)
        {
            Timer::TimerId timerId = mEvents.begin()->first.second;
            Timer timer = mEvents.begin()->second;

            timer.handleEvent();
            mEvents.erase(mEvents.begin());
            if(timer.mRepeat == true)
            {
                timer.mTimestamp = timePoint + timer.mTimeInterval;
                mEvents.insert(std::make_pair(TimerIndex(timer.mTimestamp, timerId), timer));
            }
            else
            {
                mTimers.erase(timerId);
            }
        }
    }

    modifyTimeout();
}



Timer::TimerId TimerManager::addTimer(TimerEvent* event, Timer::Timestamp timestamp,
                            Timer::TimeInterval timeInterval)
{
    Timer timer(event, timestamp, timeInterval);

    ++mLastTimerId;
    mTimers.insert(std::make_pair(mLastTimerId, timer));
    mEvents.insert(std::make_pair(TimerIndex(timestamp, mLastTimerId), timer));

    modifyTimeout();

    return mLastTimerId;
}   

bool TimerManager::removeTimer(Timer::TimerId timerId)
{
    std::map<Timer::TimerId, Timer>::iterator it = mTimers.find(timerId);
    if(it != mTimers.end())
    {
        Timer::Timestamp timestamp = it->second.mTimestamp;
        Timer::TimerId timerId = it->first;
        mEvents.erase(TimerIndex(timestamp, timerId));
        mTimers.erase(timerId);
    }

    modifyTimeout();

    return true;
}

void TimerManager::modifyTimeout()
{
    // 定时器队列采用multimap管理超时时间
    std::multimap<TimerIndex, Timer>::iterator it = mEvents.begin();
    if(it != mEvents.end())
    {
        Timer timer = it->second;
        timerFdSetTime(mTimerFd, timer.mTimestamp, timer.mTimeInterval);        
    }
    else
    {
        timerFdSetTime(mTimerFd, 0, 0);
    }
}

TimerManager::~TimerManager()
{
    mPoller->removeIOEvent(mTimerIOEvent);
    //delete mTimerIOEvent;
    // 释放定时器IO事件资源
    Delete::release(mTimerIOEvent);
}