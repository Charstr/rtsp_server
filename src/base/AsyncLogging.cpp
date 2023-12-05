#include <assert.h>
#include <mutex>
#include <stdio.h>
#include <thread>

#include "AsyncLogging.h"

#include "base/Logging.h"
#include "base/New.h"

AsyncLogging *AsyncLogging::mAsyncLogging = NULL;

AsyncLogging::AsyncLogging(std::string file) : mFile(file), mRun(true) {

	mFp = fopen(mFile.c_str(), "w");
	if (!mFp)
		return;
	for (int i = 0; i < BUFFER_NUM; ++i)
		mFreeBuffer.push(&mBuffer[i]);

	mCurBuffer = mFreeBuffer.front(); // 当前的buffer
	// 在第一次构造的时候会启动这个写日志的线程
	m_thread = std::thread([this] {
		this->run(nullptr);
	});
}

// 异步日志系统在析构的时候要确保写完
AsyncLogging::~AsyncLogging() {
	// 确保要写的都写完了
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
	if (m_thread.joinable())
		m_thread.join();
}

AsyncLogging *AsyncLogging::instance() {

	if (!mAsyncLogging) {
		mAsyncLogging = new AsyncLogging(Logger::getLogFile());
	}

	return mAsyncLogging;
}
// 要写的是从logline开始的len长度
void AsyncLogging::append(const char *logline, int len) {
	// unique_lock可以条件变量的时候释放锁
	std::unique_lock<std::mutex> lock(m_mutex);
	// 当前的这个buffer够就直接写进去
	if (mCurBuffer->avail() > len) {
		mCurBuffer->append(logline, len);
	} else {

		// 不够写就把这个添加进要刷新的队列
		mFreeBuffer.pop();
		mFlushBuffer.push(mCurBuffer);

		/* 如果缓存区已经用完，那么就睡眠直到缓冲区不为空*/

		if (mFreeBuffer.empty()) {
			m_condv.wait(lock, [this] {
				return !mFreeBuffer.empty();
			});
		};

		/* 如果缓存区已经用完，那么就睡眠等待 */
		// while (mFreeBuffer.empty()) {
		// 	m_condv.notify_one();
		// 	m_condv.wait(lock);
		// }

		// 得到一个缓冲区通知线程去执行run写日志
		mCurBuffer = mFreeBuffer.front();
		mCurBuffer->append(logline, len);
		m_condv.notify_one();
	}
}

void AsyncLogging::run(void *arg) {
	while (mRun) {
		std::unique_lock<std::mutex> lock(m_mutex);
		// std::chrono::milliseconds(3000)表示等待的最长时间为 3 秒
		// 等待 mFlushBuffer 非空或者 mRun 变为假，超时后自动结束
		bool ret = m_condv.wait_for(lock, std::chrono::milliseconds(3000), [this] {
			return !mFlushBuffer.empty() || !mRun;
		});

		if (!mRun)
			break;
		// signal
		// 说明mFlushBuffer不为空的，就要写数据进去了。
		if (ret == true) {
			bool empty = mFreeBuffer.empty();
			int bufferSize = mFlushBuffer.size();
			// 写日志
			for (int i = 0; i < bufferSize; ++i) {
				LogBuffer *buffer = mFlushBuffer.front();
				fwrite(buffer->data(), 1, buffer->length(), mFp);
				// 这一块数据写完了，就设置为能够
				mFlushBuffer.pop();
				// 进行指针偏移到起始地址设置这篇缓冲区为空
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
			// 写当前缓冲区
			fwrite(mCurBuffer->data(), 1, mCurBuffer->length(), mFp);
			mCurBuffer->reset();
			fflush(mFp);
		}
	}
}