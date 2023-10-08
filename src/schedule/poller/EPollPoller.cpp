#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "EPollPoller.h"
#include "base/Logging.h"
#include "base/New.h"

// 初始化events_的大小
static const int InitEventListSize = 16;
// 默认的Poller 超时时间,单位是毫秒
static const int epollTimeout = 10000;

EPollPoller* EPollPoller::createNew()
{
    //return new EPollPoller();
    return New<EPollPoller>::allocate();
}

EPollPoller::EPollPoller() :
    mEPollEventList(InitEventListSize)
{
    mEPollFd = ::epoll_create1(EPOLL_CLOEXEC);
}

EPollPoller::~EPollPoller()
{
    ::close(mEPollFd);
}

bool EPollPoller::addIOEvent(IOEvent* event)
{
    return updateIOEvent(event);
}

// epoll_ctl具体操作
bool EPollPoller::updateIOEvent(IOEvent* event)
{
    // 创建一个epoll事件，这些函数要学习一下
    struct epoll_event epollEvt;
    // 获取事件对应的文件描述符
    int fd = event->getFd();

    memset(&epollEvt, 0, sizeof(epollEvt));
    epollEvt.data.fd = fd;
    
    // 根据唤醒的事件类型处理
    if(event->isReadHandling())
        epollEvt.events |= EPOLLIN;
    if(event->isWriteHandling())
        epollEvt.events |= EPOLLOUT;
    if(event->isErrorHandling())
        epollEvt.events |= EPOLLERR;
        
    IOEventMap::iterator it = mEventMap.find(fd);
    
    // 事件已经注册过了，就epoll_ctl具体操作
    if(it != mEventMap.end()) {
        epoll_ctl(mEPollFd, EPOLL_CTL_MOD, fd, &epollEvt);
    }else{
        epoll_ctl(mEPollFd, EPOLL_CTL_ADD, fd, &epollEvt);
        mEventMap.insert(std::make_pair(fd, event));
        if(mEventMap.size() >= mEPollEventList.size())
            mEPollEventList.resize(mEPollEventList.size() * 2);
    }

    return true;
}

bool EPollPoller::removeIOEvent(IOEvent* event) {
    int fd = event->getFd();
    IOEventMap::iterator it = mEventMap.find(fd);
    if(it == mEventMap.end())
        return false;
    
    epoll_ctl(mEPollFd, EPOLL_CTL_DEL, fd, NULL);
    mEventMap.erase(fd);

    return true;
}

// 处理事件，对应于muduo的EPollPoller::poll函数
void EPollPoller::handleEvent() {
    
    int nums, fd, event, revent;

    // 当前需要处理的事件个数，mEPollEventList是epoll上所有的事件
    nums = epoll_wait(mEPollFd, &*mEPollEventList.begin(), mEPollEventList.size(), epollTimeout);
    if(nums < 0){
        LOG_DEBUG("epoll wait err\n");
        return;
    }

    for(int i = 0; i < nums; ++i){

        revent = 0;

        fd = (mEPollEventList.begin()+i)->data.fd;
        event = (mEPollEventList.begin()+i)->events;

        //fd = (&mEPollEventList.front()+i)->data.fd;
        // fd = mEPollEventList.at(i).data.fd;
        // event = mEPollEventList.at(i).events;
        
        // epoll返回的具体事件
        if(event & EPOLLIN || event & EPOLLPRI || event & EPOLLRDHUP)
            revent |= IOEvent::EVENT_READ;
        if(event & EPOLLOUT)
            revent |= IOEvent::EVENT_WRITE;
        if(event & EPOLLERR)
            revent |= IOEvent::EVENT_ERROR;
            
        // 在Poller::map<int, IOEvent*> mEventMap中根据文件描述符查找该事件 
  
        IOEventMap::iterator it = mEventMap.find(fd);
        assert(it != mEventMap.end());

        it->second->setREvent(revent); // 设置epoll返回的具体发生的事件类型
        mEvents.push_back(it->second);// 将发生的事件添加到集合
    }

    // 遍历所有的事件，通过不同的回调函数处理进行处理
    for(std::vector<IOEvent*>::iterator it = mEvents.begin(); it != mEvents.end(); ++it)
        (*it)->handleEvent();
    
    mEvents.clear();
}

