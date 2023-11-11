#include <stdio.h>

#include "Event.h"

std::shared_ptr<TriggerEvent> TriggerEvent::createNew(void *arg) {
	return std::make_shared<TriggerEvent>(arg);
}

std::shared_ptr<TriggerEvent> TriggerEvent::createNew() {
	return std::make_shared<TriggerEvent>(nullptr);
}

TriggerEvent::TriggerEvent(void *arg) : mArg(arg) {}
// 处理触发事件，这里是断开连接
void TriggerEvent::handleEvent() {
	if (mTriggerCallback)
		mTriggerCallback(mArg);
}

// 使用std::make_shared创建TimerEvent对象
std::shared_ptr<TimerEvent> TimerEvent::createNew(void *arg) {
	return std::make_shared<TimerEvent>(arg);
}

std::shared_ptr<TimerEvent> TimerEvent::createNew() {
	return std::make_shared<TimerEvent>(nullptr);
}

TimerEvent::TimerEvent(void *arg) : mArg(arg) {}

void TimerEvent::handleEvent() {
	// 超时回调
	if (mTimeoutCallback)
		mTimeoutCallback(mArg);
}

// 不要忘记加上作用域去掉static
std::shared_ptr<IOEvent> IOEvent::createNew(int fd, void *arg) {
	if (fd < 0)
		return nullptr;
	return std::make_shared<IOEvent>(fd, arg);
}

std::shared_ptr<IOEvent> IOEvent::createNew(int fd) {
	if (fd < 0)
		return nullptr;
	return std::make_shared<IOEvent>(fd, nullptr);
}

IOEvent::IOEvent(int fd, void *arg)
	: mFd(fd), mArg(arg), mEvent(EVENT_NONE), mREvent(EVENT_NONE), mReadCallback(NULL),
	  mWriteCallback(NULL), mErrorCallback(NULL) {}

// 处理IO事件回调
void IOEvent::handleEvent() {

	if (mReadCallback && (mREvent & EVENT_READ)) {
		mReadCallback(mArg);
	}

	if (mWriteCallback && (mREvent & EVENT_WRITE)) {
		mWriteCallback(mArg);
	}

	if (mErrorCallback && (mREvent & EVENT_ERROR)) {
		mErrorCallback(mArg);
	}
};
