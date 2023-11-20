#ifndef _EPOLLPOLLER_H_
#define _EPOLLPOLLER_H_
#include <sys/epoll.h>
#include <vector>

#include "Poller.h"

class EPollPoller : public Poller {
public:
	static EPollPoller *createNew();
	EPollPoller();
	virtual ~EPollPoller();

	// 将IOEvent对应的文件描述符和对应事件添加到epull机制
	virtual bool addIOEvent(IOEvent *event);
	virtual bool updateIOEvent(IOEvent *event);
	virtual bool removeIOEvent(IOEvent *event);

	virtual void handleEvent();

private:
	int mEPollFd;

	std::vector<epoll_event> mEPollEventList; // epoll_event集合

	std::vector<IOEvent *> mEvents; // 发生事件的集合
};

#endif //_EPOLLPOLLER_H_