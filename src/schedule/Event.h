#ifndef _EVENT_H_
#define _EVENT_H_

// #include "base/Logging.h"
#include <functional>

using EventCallback = std::function<void(void *)>;

// 触发事件的处理
class TriggerEvent {
public:
	static TriggerEvent *createNew(void *arg);
	static TriggerEvent *createNew();

	TriggerEvent(void *arg);
	~TriggerEvent(){};

	void setArg(void *arg) {
		mArg = arg;
	}
	void setTriggerCallback(EventCallback cb) {
		mTriggerCallback = cb;
	}
	void handleEvent();

private:
	void *mArg;
	EventCallback mTriggerCallback;
};

class TimerEvent {
public:
	static TimerEvent *createNew(void *arg);
	static TimerEvent *createNew();

	TimerEvent(void *arg);
	~TimerEvent() {}

	void setArg(void *arg) {
		mArg = arg;
	}

	void setTimeoutCallback(EventCallback cb) {
		mTimeoutCallback = cb;
	}
	void handleEvent();

private:
	void *mArg;
	EventCallback mTimeoutCallback;
};

class IOEvent {
public:
	enum IOEventType {
		EVENT_NONE = 0,
		EVENT_READ = 1, // 可读
		EVENT_WRITE = 2, // 可写
		EVENT_ERROR = 4, // 异常
	};

	static IOEvent *createNew(int fd, void *arg);
	static IOEvent *createNew(int fd);

	IOEvent(int fd, void *arg);
	~IOEvent() {}

	// 获取fd和events的值
	int getFd() const {
		return mFd;
	}
	int getEvent() const {
		return mEvent;
	}
	void setREvent(int event) {
		mREvent = event;
	} // used by poller
	void setArg(void *arg) {
		mArg = arg;
	}

	void setReadCallback(EventCallback cb) {
		mReadCallback = cb;
	};
	void setWriteCallback(EventCallback cb) {
		mWriteCallback = cb;
	};
	void setErrorCallback(EventCallback cb) {
		mErrorCallback = cb;
	};

	void enableReadHandling() {
		mEvent |= EVENT_READ;
	}
	void enableWriteHandling() {
		mEvent |= EVENT_WRITE;
	}
	void enableErrorHandling() {
		mEvent |= EVENT_ERROR;
	}
	void disableReadeHandling() {
		mEvent &= ~EVENT_READ;
	}
	void disableWriteHandling() {
		mEvent &= ~EVENT_WRITE;
	}
	void disableErrorHandling() {
		mEvent &= ~EVENT_ERROR;
	}

	bool isNoneHandling() const {
		return mEvent == EVENT_NONE;
	}
	bool isReadHandling() const {
		return (mEvent & EVENT_READ) != 0;
	}
	bool isWriteHandling() const {
		return (mEvent & EVENT_WRITE) != 0;
	}
	bool isErrorHandling() const {
		return (mEvent & EVENT_ERROR) != 0;
	};

	// 分别对上边的三种事件调用各自的回调函数进行处理
	void handleEvent();

private:
	int mFd; // fd,poller监听的对象，当前事件对应的描述符
	void *mArg;
	// 在事件处理过程中比较当前事件和触发事件，以确定事件的状态和类型。
	int mEvent; // 当前事件类型，通常设置为希望监听的事件类型，如可读（EPOLLIN）、可写（EPOLLOUT）等。
	int mREvent; // poller返回的具体发生的事件

	EventCallback mReadCallback; // 可读事件回调函数
	EventCallback mWriteCallback; // 可写事件回调函数
	EventCallback mErrorCallback; // 错误事件回调函数
};

#endif //_EVENT_H_