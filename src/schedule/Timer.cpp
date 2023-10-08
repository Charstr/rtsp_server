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
    mRepeat = timeInterval > 0 ? true : false;
}

Timer::~Timer(){}

// 获取当前时间的毫秒表示
Timer::Timestamp Timer::getCurTime(){
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    return (now.tv_sec*1000 + now.tv_nsec/1000000);
}

void Timer::handleEvent(){
    if(!mTimerEvent) return;

    mTimerEvent->handleEvent();// 调用TimerEvent对象的事件处理函数
}

TimerManager* TimerManager::createNew(Poller* poller) {
    if(!poller) return nullptr;

    // Linux提供的定时器timerfd_create，将定时器文件描述符作为一个事件交给Reactor
    // 定时器队列采用管理超时时间
    // 消费者有一个定时器，间隔一定时间就会向生产者取数据，并将数据RTP打包再传输
    // 时间的文件描述符
    int timerFd = timerFdCreate(CLOCK_MONOTONIC,
                                TFD_NONBLOCK | TFD_CLOEXEC);
    if(timerFd < 0){
        LOG_ERROR("failed to create timer fd\n");
        return nullptr;
    }

    //return new TimerManager(timerFd, poller);
    return New<TimerManager>::allocate(timerFd, poller);
}

TimerManager::TimerManager(int timerFd, Poller* poller) :
    mTimerFd(timerFd),
    mPoller(poller),
    mLastTimerId(0)
{   

    // 根据定时器fd创建 定时器IO事件
    mTimerIOEvent = IOEvent::createNew(mTimerFd, this);

    // 设置处理定时任务的回调函数
    mTimerIOEvent->setReadCallback(TimerManager::handleRead);
    mTimerIOEvent->enableReadHandling();
    modifyTimeout(); // 修正时间？
    
    // 将定时器IO事件添加到事件循环中
    mPoller->addIOEvent(mTimerIOEvent);
}


// 定时事件mTimerIOEvent触发时候调用的回调函数
void TimerManager::handleRead(void* arg){
    if(!arg) return;

    TimerManager* timerManager = (TimerManager*)arg;
    timerManager->handleTimerEvent();
}

// 处理定时事件
void TimerManager::handleTimerEvent(){

    if(!mTimers.empty()){ // 存储的添加的定时器触发事件

        int64_t timePoint = Timer::getCurTime(); // 当前时间
        // TimerId定时器ID，Timer单个定时器，Timestamp时间戳
        // map<TimerId, Timer> mTimers; 定时器ID和定时器映射
        // multimap<pair<Timestamp, TimerId>, Timer> mEvents; 与定时器事件对应的事件

        // mEvents是按照时间顺序排列的，所以可以直接取出来第一个处理
        // 遍历定时器中定时少于当前时间的进行处理，到了定时事件，就发送rtp包
        while(!mTimers.empty() && mEvents.begin()->first.first <= timePoint){

            Timer::TimerId timerId = mEvents.begin()->first.second; // 定时器ID
            Timer timer = mEvents.begin()->second; // 单个定时器
            // 通过设置的定时事件回调函数处理事件
            timer.handleEvent(); 
            mEvents.erase(mEvents.begin()); // 执行完之后删除该事件
            // 如果事件要重复执行就更新下次执行的事件戳加入到存储事件的mEvents中。
            if(timer.mRepeat == true) {
                timer.mTimestamp = timePoint + timer.mTimeInterval; // 下次执行的时间戳
                // 按照时间戳排序的事件
                mEvents.insert(std::make_pair(TimerIndex(timer.mTimestamp, timerId), timer));
            
            }else mTimers.erase(timerId); // 不需要重复就删除对应的定时器
   
        }
    }

    modifyTimeout();
}


// 添加定时发生的事件
Timer::TimerId TimerManager::addTimer(TimerEvent* event, Timer::Timestamp timestamp,
                            Timer::TimeInterval timeInterval)
{
    Timer timer(event, timestamp, timeInterval); // 创建定时器

    ++mLastTimerId; // 新添加的定时器索引
    // 存储要触发的定时器，按照时间顺序加入的
    mTimers.insert(std::make_pair(mLastTimerId, timer));
    // 与上边定时器相关的事件，按照时间顺序加入的
    mEvents.insert(std::make_pair(TimerIndex(timestamp, mLastTimerId), timer));

    modifyTimeout();

    return mLastTimerId;
}   

bool TimerManager::removeTimer(Timer::TimerId timerId)
{
    std::map<Timer::TimerId, Timer>::iterator it = mTimers.find(timerId);
    if(it != mTimers.end()){
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
    if(it != mEvents.end()){
        Timer timer = it->second;
        timerFdSetTime(mTimerFd, timer.mTimestamp, timer.mTimeInterval);        
    } else timerFdSetTime(mTimerFd, 0, 0);

}

TimerManager::~TimerManager(){
    mPoller->removeIOEvent(mTimerIOEvent);
    // 释放定时器IO事件资源
    Delete::release(mTimerIOEvent);
}