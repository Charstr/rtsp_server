#include <arpa/inet.h>

#include "RtpSink.h"
#include "base/Logging.h"
#include "base/New.h"

// 消费者，用于将音视频数据进行RTP打包，然后发送给客户端
RtpSink::RtpSink(UsageEnvironment* env, MediaSource* mediaSource, int payloadType) :
    mMediaSource(mediaSource),
    mSendPacketCallback(NULL),
    mEnv(env),
    mCsrcLen(0),
    mExtension(0),
    mPadding(0),
    mVersion(RTP_VESION),
    mPayloadType(payloadType),
    mMarker(0),
    mSeq(0),
    mTimestamp(0),
    mTimerId(0)
    
{
    mTimerEvent = TimerEvent::createNew(this);
    // 设置超时回调函数RtpSink::timeoutCallback
    // 发送数据
    mTimerEvent->setTimeoutCallback(RtpSink::timeoutCallback);

    mSSRC = rand();
}

RtpSink::~RtpSink()
{
    mEnv->scheduler()->removeTimedEvent(mTimerId);
    //delete mTimerEvent;
    Delete::release(mTimerEvent);
}

// 设置发送数据包的回调函数。
void RtpSink::setSendFrameCallback(SendPacketCallback cb, void* arg1, void* arg2)
{
    mSendPacketCallback = cb;
    mArg1 = arg1;
    mArg2 = arg2;
}

// 发送RTP数据包的函数，会给数据包设置相应的头部信息，并调用发送数据包的回调函数。
void RtpSink::sendRtpPacket(RtpPacket* packet)
{
    RtpHeader* rtpHead = packet->mRtpHeadr;
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
    
    if(RtpSink::mSendPacketCallback)
        RtpSink::mSendPacketCallback(mArg1, mArg2, packet);
}

// 超时回调函数，用于处理帧数据并发送到目标位置。
void RtpSink::timeoutCallback(void* arg)
{
    RtpSink* rtpSink = (RtpSink*)arg;
    AVFrame* frame = rtpSink->mMediaSource->getFrame();
    if(!frame)
    {
        return;
    }

    // 处理输出
    rtpSink->handleFrame(frame);
    // 循环队列，重复利用
    rtpSink->mMediaSource->putFrame(frame);
}

// 启动定时事件，以指定的时间间隔调用timeoutCallback函数。
void RtpSink::start(int ms)
{
    mTimerId = mEnv->scheduler()->addTimedEventRunEvery(mTimerEvent, ms);
}

void RtpSink::stop()
{
    mEnv->scheduler()->removeTimedEvent(mTimerId);
}