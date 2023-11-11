#ifndef _MEDIA_SOURCE_H_
#define _MEDIA_SOURCE_H_
#include <mutex>
#include <queue>
#include <stdint.h>

#include "schedule/UsageEnvironment.h"

#define FRAME_MAX_SIZE (1024 * 500) // 最大视频帧大小
#define DEFAULT_FRAME_NUM 4 // 默认缓冲帧数量

/*
1.
MediaSource类维护了一个由AVFrame对象构成的视频帧缓冲池，其中mAVFrames数组用于存储视频帧数据，mAVFrameInputQueue用于管理待处理的视频帧，而mAVFrameOutputQueue用于存储已处理的视频帧。

2.
获取已处理的视频帧，从输出队列中取出一个视频帧并返回。putFrame函数用于将新的视频帧放入待处理队列mAVFrameInputQueue中。
3.
MediaSource类利用线程池来处理视频帧，当新的视频帧放入待处理队列时，它会创建一个任务（mTask），并将任务添加到线程池中。这样，视频帧的读取和处理可以并发执行。

4.
MediaSource是用于管理视频帧的生产者，负责缓冲视频帧并放入待处理队列。使用线程池来异步处理视频帧，实现了生产者与消费者模式，用于数据采集与处理。可以提高多线程处理视频帧的效率和并发性。
*/

class AVFrame {
public:
	// 分配视频帧缓冲区
	AVFrame() : mBuffer(new uint8_t[FRAME_MAX_SIZE]), mFrameSize(0) {}

	~AVFrame() {
		delete[] mBuffer;
	}

	uint8_t *mBuffer; // 视频帧数据缓冲区
	uint8_t *mFrame; // 视频帧数据指针
	int mFrameSize; // AVFrame大小
};

class MediaSource {
public:
	virtual ~MediaSource();

	AVFrame *getFrame(); // 获取视频帧
	void putFrame(AVFrame *frame); // 放入视频帧
	int getFps() const {
		return mFps;
	}

protected:
	MediaSource(UsageEnvironment *env);
	virtual void readFrame() = 0; // 读取视频帧的抽象接口
	void setFps(int fps) {
		mFps = fps;
	}

private:
	// 回调产生数据
	static void taskCallback(void *);

protected:
	UsageEnvironment *mEnv;
	/*
	这里设计了一个环形队列，环形队列的大小是缓冲区的大小。入队列和出队列都维护指向缓冲区的指针
	生产数据的时候，从h264文件读取一个AVFrame到入队列，然后入队列取出一个AVFrame插入到出队列

	消费数据的时候，getFrame从出队列取出一个AVFrame然后插入到入队列
	*/

	AVFrame mAVFrames[DEFAULT_FRAME_NUM]; // 视频帧缓冲区数组
	std::queue<AVFrame *> mAVFrameInputQueue; // 视频帧输入队列
	std::queue<AVFrame *> mAVFrameOutputQueue; // 视频帧输出队列
	std::mutex m_mutex;
	std::function<void()> mTask; // 线程池任务
	int mFps;
};

#endif //_MEDIA_SOURCE_H_