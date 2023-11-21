#ifndef _ASYNLOGGING_H_
#define _ASYNLOGGING_H_
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string.h>
#include <string>
#include <thread>

class LogBuffer {
public:
	LogBuffer() : mCurPtr(mData) {}

	~LogBuffer() {}

	void append(const char *buf, size_t len) {
		if (avail() > len) {
			memcpy(mCurPtr, buf, len);
			mCurPtr += len;
		}
	}

	const char *data() const {
		return mData;
	}
	int length() const {
		return (int)(mCurPtr - mData);
	}

	char *current() {
		return mCurPtr;
	}
	int avail() const {
		return (int)(end() - mCurPtr);
	}
	void add(int len) {
		mCurPtr += len;
	}

	void reset() {
		mCurPtr = mData;
	}
	void bzero() {
		memset(mData, 0, BUFFER_SIZE);
	}

private:
	enum {
		BUFFER_SIZE = 1024 * 1024,
	};

	const char *end() const {
		return mData + BUFFER_SIZE;
	}

private:
	char mData[BUFFER_SIZE]; // 存储日志数据。
	char *mCurPtr;
};

// 负责实际的日志记录工作
class AsyncLogging {
public:
	virtual ~AsyncLogging();
	// 只能有一个AsyncLogging实例
	static AsyncLogging *instance();
	// 将日志数据追加到正在处理的缓冲区中。
	void append(const char *logline, int len);

protected:
	AsyncLogging(std::string file);
	virtual void run(void *arg);

private:
	enum {
		BUFFER_NUM = 4,
	};

	std::mutex m_mutex;
	std::condition_variable m_condv;
	std::string mFile;
	FILE *mFp;
	bool mRun;
	// 缓冲区
	LogBuffer mBuffer[BUFFER_NUM];
	LogBuffer *mCurBuffer;
	// 两个队列,当需要记录日志时，AsyncLogging类会从空闲缓冲区中获取一个缓冲区，并将日志数据写入其中。当缓冲区满时，它将缓冲区添加到需要写入文件的缓冲区队列中，并触发写入文件的操作。
	std::queue<LogBuffer *> mFreeBuffer;
	std::queue<LogBuffer *> mFlushBuffer;
	std::thread m_thread;
	static AsyncLogging *mAsyncLogging;
};

#endif //_ASYNLOGGING_H_