#ifndef _EVENT_H_
#define _EVENT_H_
#include <functional>
#include <memory>

using EventCallback = std::function<void(void *)>;

class TriggerEvent : public std::enable_shared_from_this<TriggerEvent> {
public:
	static std::shared_ptr<TriggerEvent> createNew(void *arg);
	static std::shared_ptr<TriggerEvent> createNew();

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
class TimerEvent : public std::enable_shared_from_this<TimerEvent> {
public:
	static std::shared_ptr<TimerEvent> createNew(void *arg);
	static std::shared_ptr<TimerEvent> createNew();

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

// 这个IOEvent对应于muduo的channel
class IOEvent : public std::enable_shared_from_this<IOEvent> {
public:
	enum IOEventType {
		EVENT_NONE = 0,
		EVENT_READ = 1, // 可读
		EVENT_WRITE = 2, // 可写
		EVENT_ERROR = 4, // 异常
	};

	static std::shared_ptr<IOEvent> createNew(int fd, void *arg);
	static std::shared_ptr<IOEvent> createNew(int fd);

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

	EventCallback mReadCallback;
	EventCallback mWriteCallback;
	EventCallback mErrorCallback;
};

#endif //_EVENT_H_