#include <stdio.h>

#include "Event.h"
#include "base/New.h"

TriggerEvent *TriggerEvent::createNew(void *arg) {
	return New<TriggerEvent>::allocate(arg);
}

TriggerEvent *TriggerEvent::createNew() {
	return New<TriggerEvent>::allocate((void *)0);
}

TriggerEvent::TriggerEvent(void *arg) : mArg(arg) {}

// 处理触发事件，这里是断开连接
void TriggerEvent::handleEvent() {
	if (mTriggerCallback)
		mTriggerCallback(mArg);
}

TimerEvent *TimerEvent::createNew(void *arg) {
	return New<TimerEvent>::allocate(arg);
}

TimerEvent *TimerEvent::createNew() {
	return New<TimerEvent>::allocate((void *)0);
}

TimerEvent::TimerEvent(void *arg) : mArg(arg) {}

void TimerEvent::handleEvent() {
	if (mTimeoutCallback)
		mTimeoutCallback(mArg);
}

IOEvent *IOEvent::createNew(int fd, void *arg) {
	if (fd < 0)
		return nullptr;
	return New<IOEvent>::allocate(fd, arg);
}

IOEvent *IOEvent::createNew(int fd) {
	if (fd < 0)
		return nullptr;
	return New<IOEvent>::allocate(fd, (void *)0);
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
