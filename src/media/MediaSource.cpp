#include "MediaSource.h"
#include "base/Logging.h"
#include "base/New.h"
#include <cstddef>
#include <mutex>

MediaSource::MediaSource(UsageEnvironment *env) : mEnv(env) {

	/*

	视频帧缓冲区mAVFrames大小为DEFAULT_FRAME_NUM，也就是环形队列的大小。初始化的时候把mAVFrames每个元素的指针插入到mAVFrameInputQueue，缓冲区所在的位置是固定的，在mAVFrameInputQueue和mAVFrameOutputQueue之间传递的是缓冲区的指针，所以就是缓冲区的数据承载着读取的一个AVFrame，其指针存在两个队列中互相流转

	环形队列怎么生效的：
	1.
	线程池中的每个线程执行的任务mTask通过回调都会调用readFrame从文件序号读取视频帧生产数据,存在mAVFrameInputQueue，然后再push给mAVFrameOutputQueue。
	2.
	当定时事件mTimerEvent触发的时候，会回调设置的mTimerEvent->setTimeoutCallback(RtpSink::timeoutCallback);RtpSink::timeoutCallback开始消费数据。getFrame从mAVFrameOutputQueue读取视频帧，然后rtpSink->handleFrame通过rtp包发送出去，再putFrame将读取的帧插入mAVFrameInputQueue。
	这样就构成了一个环形队列
	*/

	for (int i = 0; i < DEFAULT_FRAME_NUM; ++i)
		mAVFrameInputQueue.push(&mAVFrames[i]);
}

MediaSource::~MediaSource() {}

// RtpSink调用getFrame和putFrame函数进行数据消费

AVFrame *MediaSource::getFrame() {
	std::lock_guard<std::mutex> lock(m_mutex);
	// 输出队列为空，返回空指针
	if (mAVFrameOutputQueue.empty())
		return nullptr;

	// 从输出队列中取出一个视频帧
	AVFrame *frame = std::move(mAVFrameOutputQueue.front());
	mAVFrameOutputQueue.pop();

	return frame;
}

// 将一个AVFrame对象放入输入队列，然后将一个任务添加到线程池。
void MediaSource::putFrame(AVFrame *frame) {
	std::lock_guard<std::mutex> lock(m_mutex);

	mAVFrameInputQueue.push(frame); // 将视频帧放入待处理队列

	// 每当一个新的帧被放入输入队列时，就需要一个新的任务来处理它，都会添加一个新的任务到任务队列，
	mEnv->threadPool()->addTask(std::bind(&MediaSource::readFrame, this));
}
