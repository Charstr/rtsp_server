#ifndef _MEDIA_SINK_H_
#define _MEDIA_SINK_H_
#include <stdint.h>
#include <string>

#include "MediaSource.h"
#include "schedule/Event.h"
#include "schedule/UsageEnvironment.h"
#include "server/Rtp.h"

/*
用于实现RTP（Real-time Transport
Protocol）数据包的发送，支持了RTP头部字段的设置，包括版本、负载类型、序列号、时间戳等。
RtpSink类使用定时器来定期发送RTP数据包
通过定时器回调函数，媒体数据的发送被安排在后台线程中，以避免阻塞主线程。这有助于保持媒体数据的实时性。
*/
class RtpSink {
public:
	// 回调函数类型定义，用于发送RTP数据包
	typedef void (*SendPacketCallback)(void *arg1, void *arg2, RtpPacket *mediaPacket);
	// 构造函数
	RtpSink(UsageEnvironment *env, MediaSource *mediaSource, int payloadType);
	virtual ~RtpSink();
	// 抽象函数，用于获取媒体描述，子类需要实现
	virtual std::string getMediaDescription(uint16_t port) = 0;
	// 抽象函数，用于获取属性，子类需要实现
	virtual std::string getAttribute() = 0;
	// 当媒体帧准备好时，回调函数将被触发，允许将媒体数据打包成RTP数据包发送，默认在cpp实现
	void setSendFrameCallback(SendPacketCallback cb, void *arg1, void *arg2) {
		mSendPacketCallback = cb;
		mArg1 = arg1;
		mArg2 = arg2;
	}

protected:
	// 抽象函数，用于处理媒体帧，子类需要实现
	virtual void handleFrame(AVFrame *frame) = 0;
	void sendRtpPacket(RtpPacket *packet); // 发送RTP数据包
	void start(int ms); // 启动RTP发送定时器，以指定的毫秒间隔发送数据
	void stop(); // 停止RTP发送定时器

private:
	// 静态定时器回调函数，当定时器超时时调用，不能调用非静态成员变量
	// 改成非静态
	static void timeoutCallback(void *);

protected:
	UsageEnvironment *mEnv; // 使用的环境
	MediaSource *mMediaSource; // 媒体源,多态
	SendPacketCallback mSendPacketCallback; // 发送帧回调函数
	void *mArg1;
	void *mArg2;

	// RTP头字段
	uint8_t mCsrcLen;
	uint8_t mExtension;
	uint8_t mPadding;
	uint8_t mVersion;
	uint8_t mPayloadType;
	uint8_t mMarker;
	uint16_t mSeq;
	uint32_t mTimestamp;
	uint32_t mSSRC;

private:
	TimerEvent *mTimerEvent; // 定时器事件，定时触发进行数据的发送
	Timer::TimerId mTimerId; // 定时器ID，上边是对应的定时器事件
};

#endif //_MEDIA_SINK_H_