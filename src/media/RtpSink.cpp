#include <arpa/inet.h>
#include <functional>
#include <mutex>

#include "RtpSink.h"
#include "base/Logging.h"
#include "base/New.h"
#include "schedule/UsageEnvironment.h"

// 消费者，用于将音视频数据进行RTP打包，然后发送给客户端
RtpSink::RtpSink(UsageEnvironment *env, MediaSource *mediaSource, int payloadType)
	: mMediaSource(mediaSource), mSendPacketCallback(NULL), mEnv(env), mCsrcLen(0), mExtension(0),
	  mPadding(0), mVersion(RTP_VESION), mPayloadType(payloadType), mMarker(0), mSeq(0),
	  mTimestamp(0), mTimerId(0), mSSRC(rand()) {

	// sink是消费者，创建一个定时器事件，用于定时器触发的
	mTimerEvent = TimerEvent::createNew(this);

	// 这里也考虑加入到线程池？
	// 设置超时回调函数RtpSink::timeoutCallback，达到定时触发的时候从mAVFrameOutputQueue帧输出队列取出来一帧，
	// 通过多态调用H264RtpSink::handleFrame函数发送一个rtp packet

	// mTimerEvent->setTimeoutCallback([env](void *arg) {
	// 	RtpSink::timeoutCallback(arg, env);
	// });
	mTimerEvent->setTimeoutCallback(RtpSink::timeoutCallback);
}
RtpSink::~RtpSink() {
	mEnv->scheduler()->removeTimedEvent(mTimerId);
	Delete::release(mTimerEvent);
}

// 发送RTP数据包的函数，会给数据包设置相应的头部信息，并调用发送数据包的回调函数。
// 具体的rtpsink调用
void RtpSink::sendRtpPacket(RtpPacket *packet) {
	RtpHeader *rtpHead = packet->mRtpHeadr;
	rtpHead->csrcLen = mCsrcLen;
	rtpHead->extension = mExtension;
	rtpHead->padding = mPadding;
	rtpHead->version = mVersion;
	rtpHead->payloadType = mPayloadType;
	rtpHead->marker = mMarker;
	rtpHead->seq = htons(mSeq);
	rtpHead->timestamp = htonl(mTimestamp);
	rtpHead->ssrc = htonl(mSSRC);
	packet->mSize += RTP_HEADER_SIZE;
	// 回调函数发送rtp数据包，设置的是MediaSession::sendPacketCallback函数
	if (RtpSink::mSendPacketCallback)
		RtpSink::mSendPacketCallback(mArg1, mArg2, packet);
}

// 超时回调函数，从唤醒队列取出来一帧发送然后再重复利用
// 这时候加锁，取出来一个然后发送
std::mutex mmutex;
void RtpSink::timeoutCallback(void *arg) {
	// 发生超时的时候，加入到线程池
	RtpSink *rtpSink = (RtpSink *)arg;
	// 超时从输出队列mAVFrameOutputQueue取出一个AVFrame
	AVFrame *frame = rtpSink->mMediaSource->getFrame();
	if (!frame)
		return;

	// 这是在主线程完成的，现在想加入到线程池
	// 多态发送一个AVFrame，调用H264RtpSink::handleFrame
	// 多线程的分别完成
	rtpSink->handleFrame(frame);
	// 尝试往线程池添加任务
	// 是否能行？
	// {
	// 	std::unique_lock<std::mutex> lock(mmutex);
	// 	env->threadPool()->addTask(std::bind(&RtpSink::handleFrame, rtpSink, frame));
	// }

	// 循环队列，重复利用，此时会向线程池的任务队列添加一个任务
	rtpSink->mMediaSource->putFrame(frame);
}

// 添加定时器，timeoutCallback是定时器处理的回调函数
void RtpSink::start(int ms) {
	mTimerId = mEnv->scheduler()->addTimedEventRunEvery(mTimerEvent, ms);
}

void RtpSink::stop() {
	mEnv->scheduler()->removeTimedEvent(mTimerId);
}