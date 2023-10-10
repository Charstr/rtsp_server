#ifndef _EPOLLPOLLER_H_
#define _EPOLLPOLLER_H_
#include <sys/epoll.h>
#include <vector>

#include "Poller.h"

// epoll实现的多路事件分发器，内部实现依赖epoll相关的系统调用(epoll_create,epoll_ctl,epoll_wait)

class EPollPoller : public Poller
{
public:
    // 工厂函数用于实现默认的poller
    static EPollPoller* createNew();

    EPollPoller();
    virtual ~EPollPoller();

    // 将IOEvent对应的文件描述符和对应事件添加到epull机制
    virtual bool addIOEvent(IOEvent* event);
    virtual bool updateIOEvent(IOEvent* event);
    virtual bool removeIOEvent(IOEvent* event);

    // 对应muduo的poll函数，epoll_wait轮询得到当前要处理的事件数目进行处理。
    virtual void handleEvent();

private:
    int mEPollFd;

    std::vector<epoll_event> mEPollEventList; // epoll_event集合

    std::vector<IOEvent*> mEvents; // 发生事件的集合
};

#endif //_EPOLLPOLLER_H_