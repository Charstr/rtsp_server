#ifndef _POLLER_H_
#define _POLLER_H_
#include "schedule/Event.h"
#include <map>

class Poller // 抽象基类，用于实现多路复用分发器
{
public:
	virtual ~Poller();

	// 不同的IO复用方法继承虚基类后实现
	virtual bool addIOEvent(IOEvent *event) = 0;
	virtual bool updateIOEvent(IOEvent *event) = 0;
	virtual bool removeIOEvent(IOEvent *event) = 0;

	// 给select,poll,epoll保留统一的接口，是实现IO复用的函数,用于等待事件的发生
	virtual void handleEvent() = 0;

protected:
	Poller();

protected:
	typedef std::map<int, IOEvent *> IOEventMap;
	// 维护文件描述符与IOEvent对象的映射关系,保管所有注册在这个poller上的IOEvent
	IOEventMap mEventMap;
};

#endif //_POLLER_H_