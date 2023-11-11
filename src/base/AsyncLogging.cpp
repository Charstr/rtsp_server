#include <assert.h>
#include <mutex>
#include <stdio.h>
#include <thread>

#include "AsyncLogging.h"

#include "base/Logging.h"
#include "base/New.h"

/*
日志记录系统的头文件

*/
AsyncLogging *AsyncLogging::mAsyncLogging = NULL;

AsyncLogging::AsyncLogging(std::string file) : mFile(file), mRun(true) {

	mFp = fopen(mFile.c_str(), "w");
	if (!mFp)
		return;
	for (int i = 0; i < BUFFER_NUM; ++i) mFreeBuffer.push(&mBuffer[i]);

	mCurBuffer = mFreeBuffer.front();

	// 这个线程是干什么的？
	m_thread = std::thread([this] {
		this->run(nullptr);
	});
}

AsyncLogging::~AsyncLogging() {
	for (int i = 0; i < mFlushBuffer.size(); ++i) {
		LogBuffer *buffer = mFlushBuffer.front();
		fwrite(buffer->data(), 1, buffer->length(), mFp);
		mFlushBuffer.pop();
	}

	fwrite(mCurBuffer->data(), 1, mCurBuffer->length(), mFp);

	fflush(mFp);
	fclose(mFp);
	if (mAsyncLogging) {
		delete mAsyncLogging;
		mAsyncLogging = nullptr;
	}

	mRun = false;
	m_condv.notify_all();
}

AsyncLogging *AsyncLogging::instance() {
	if (!mAsyncLogging) {
		mAsyncLogging = new AsyncLogging(Logger::getLogFile());
	}

	return mAsyncLogging;
}
void AsyncLogging::append(const char *logline, int len)

{
	std::unique_lock<std::mutex> lock(m_mutex);
	if (mCurBuffer->avail() > len) {
		mCurBuffer->append(logline, len);
	} else {
		mFreeBuffer.pop();
		mFlushBuffer.push(mCurBuffer);

		/* 如果缓存区已经用完，那么就睡眠等待 */
		while (mFreeBuffer.empty()) {
			m_condv.notify_one();
			m_condv.wait(lock);
		}

		mCurBuffer = mFreeBuffer.front();
		mCurBuffer->append(logline, len);
		m_condv.notify_one();
	}
}

void AsyncLogging::run(void *arg) {
	while (mRun) {
		std::unique_lock<std::mutex> lock(m_mutex);
		bool ret = m_condv.wait_for(lock, std::chrono::milliseconds(3000), [this] {
			return !mFlushBuffer.empty() || !mRun;
		});

		if (!mRun)
			break;
		// signal
		if (ret == true) {
			bool empty = mFreeBuffer.empty();
			int bufferSize = mFlushBuffer.size();
			for (int i = 0; i < bufferSize; ++i) {
				LogBuffer *buffer = mFlushBuffer.front();
				fwrite(buffer->data(), 1, buffer->length(), mFp);
				mFlushBuffer.pop();
				buffer->reset();
				mFreeBuffer.push(buffer);
				fflush(mFp);
			}
			if (empty)
				m_condv.notify_one();
		} else {
			// timeout
			if (mCurBuffer->length() == 0)
				continue;
			fwrite(mCurBuffer->data(), 1, mCurBuffer->length(), mFp);
			mCurBuffer->reset();
			fflush(mFp);
		}
	}
}