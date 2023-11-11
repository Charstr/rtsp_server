#ifndef _AAC_RTP_SINK_H_
#define _AAC_RTP_SINK_H_

#include "MediaSource.h"
#include "RtpSink.h"
#include "schedule/UsageEnvironment.h"

// 继承自RtpSink，提供了一些方法和成员变量来处理AAC（Advanced Audio Coding）格式的音频数据。
class AACRtpSink : public RtpSink {
public:
	static AACRtpSink *createNew(UsageEnvironment *env, MediaSource *mediaSource);

	AACRtpSink(UsageEnvironment *env, MediaSource *mediaSource, int payloadType);
	virtual ~AACRtpSink();

	virtual std::string getMediaDescription(uint16_t port);
	virtual std::string getAttribute();

protected:
	// 用于处理帧。
	virtual void handleFrame(AVFrame *frame);

private:
	RtpPacket mRtpPacket; // 存储RTP（Real-time Transport Protocol）数据包。
	uint32_t mSampleRate; // 采样频率
	uint32_t mChannels; // 通道数
	int mFps;
};

#endif //_AAC_RTP_SINK_H_