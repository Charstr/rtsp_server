#ifndef _EVENT_H_
#define _EVENT_H_

// #include "base/Logging.h"

typedef void (*EventCallback)(void *);

/*
事件相关的头文件，定义了三个类：`TriggerEvent`、`TimerEvent`和`IOEvent`。

`TriggerEvent`类用于触发事件
`TimerEvent`类用于定时事件
`IOEvent`类用于处理IO事件，包含一个文件描述符`int
fd`和一个`void*`类型的参数，还有读、写、错误三种类型的事件回调函数指针。该类提供了一系列设置和获取事件状态、处理事件等方法。

TriggerEvent和TimerEvent类似，区别在于用途不同，一个是触发事件，一个是定时事件。

IOEvent用于处理I/O事件，可以处理可读、可写和错误事件，并具有相应的回调函数。
每个事件都附带回调函数和参数，能够触发相应的事件回调函数。通过createNew函数创建事件对象，使用handleEvent方法处理事件。

*/

// 触发事件的处理
class TriggerEvent {
public:
	static TriggerEvent *createNew(void *arg);
	static TriggerEvent *createNew();

	TriggerEvent(void *arg);
	~TriggerEvent(){};
	// `void*`类型的参数和一个回调函数指针，可以自定义事件的处理逻辑。
	void setArg(void *arg) {
		mArg = arg;
	}
	void setTriggerCallback(EventCallback cb) {
		mTriggerCallback = cb;
	}
	void handleEvent();

private:
	void *mArg; // 回调函数参数
	EventCallback mTriggerCallback; // 触发事件回调函数
};

// 定时事件的处理
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