#include <memory>
#include <stdio.h>
#include <string.h>

#include "H264RtpSink.h"
// #include "base/Logging.h"
#include "base/New.h"
#include "media/RtpSink.h"

std::shared_ptr<H264RtpSink>
H264RtpSink::createNew(UsageEnvironment *env, MediaSource *mediaSource) {
	if (!mediaSource)
		return nullptr;
	return std::make_shared<H264RtpSink>(env, mediaSource);
}

H264RtpSink::H264RtpSink(UsageEnvironment *env, MediaSource *mediaSource)
	: RtpSink(env, mediaSource, RTP_PAYLOAD_TYPE_H264), mClockRate(90000),
	  mFps(mediaSource->getFps()) {

	// 在 RtpSink构造函数创建了定时器事件mTimerEvent，start时候加入到定时器的列表中
	// 1秒/帧率得到定时器的触发间隔
	start(1000 / mFps);
}

H264RtpSink::~H264RtpSink() {}

// 根据给定的端口号和负载类型生成媒体描述。
std::string H264RtpSink::getMediaDescription(uint16_t port) {
	char buf[100] = {0};
	sprintf(buf, "m=video %hu RTP/AVP %d", port, mPayloadType);

	return std::string(buf);
}

std::string H264RtpSink::getAttribute() {
	char buf[100];
	sprintf(buf, "a=rtpmap:%d H264/%d\r\n", mPayloadType, mClockRate);
	sprintf(buf + strlen(buf), "a=framerate:%d", mFps);

	return std::string(buf);
}

// 根据帧的大小和类型发送RTP包，传进来的帧是mAVFrameOutputQueue取出的AVFrame
void H264RtpSink::handleFrame(AVFrame *frame) {

	RtpHeader *rtpHeader = mRtpPacket.mRtpHeadr;
	uint8_t naluType = frame->mFrame[0];

	// 一个pkt能发完
	if (frame->mFrameSize <= RTP_MAX_PKT_SIZE) {
		memcpy(rtpHeader->payload, frame->mFrame, frame->mFrameSize);
		mRtpPacket.mSize = frame->mFrameSize;
		sendRtpPacket(&mRtpPacket);
		mSeq++;

		if ((naluType & 0x1F) == 7 || (naluType & 0x1F) == 8) // 如果是SPS、PPS就不需要加时间戳
			return;
	} else {
		// 拆分成几个发送
		int pktNum = frame->mFrameSize / RTP_MAX_PKT_SIZE; // 有几个完整的包
		int remainPktSize = frame->mFrameSize % RTP_MAX_PKT_SIZE; // 剩余不完整包的大小
		int i, pos = 1;

		/* 发送完整的包 */
		for (i = 0; i < pktNum; i++) {
			/*
			 *     FU Indicator
			 *    0 1 2 3 4 5 6 7
			 *   +-+-+-+-+-+-+-+-+
			 *   |F|NRI|  Type   |
			 *   +---------------+
			 * */
			rtpHeader->payload[0] =
				(naluType & 0x60) | 28; //(naluType & 0x60)表示nalu的重要性，28表示为分片

			/*
			 *      FU Header
			 *    0 1 2 3 4 5 6 7
			 *   +-+-+-+-+-+-+-+-+
			 *   |S|E|R|  Type   |
			 *   +---------------+
			 * */
			rtpHeader->payload[1] = naluType & 0x1F;

			if (i == 0) // 第一包数据
				rtpHeader->payload[1] |= 0x80; // start
			else if (remainPktSize == 0 && i == pktNum - 1) // 最后一包数据
				rtpHeader->payload[1] |= 0x40; // end

			memcpy(rtpHeader->payload + 2, frame->mFrame + pos, RTP_MAX_PKT_SIZE);
			mRtpPacket.mSize = RTP_MAX_PKT_SIZE + 2;
			// 发送rtp包，调用回调函数RtpSink::mSendPacketCallback发送
			sendRtpPacket(&mRtpPacket);
			// RtpSink::sendRtpPacket(&mRtpPacket);
			mSeq++;
			pos += RTP_MAX_PKT_SIZE;
		}

		/* 发送剩余的数据 */
		// 多的单独一个pkt发送
		if (remainPktSize > 0) {
			rtpHeader->payload[0] = (naluType & 0x60) | 28;
			rtpHeader->payload[1] = naluType & 0x1F;
			rtpHeader->payload[1] |= 0x40; // end

			memcpy(rtpHeader->payload + 2, frame->mFrame + pos, remainPktSize);
			mRtpPacket.mSize = remainPktSize + 2;
			sendRtpPacket(&mRtpPacket);
			mSeq++;
		}
	}
	// 更新时间戳
	mTimestamp += mClockRate / mFps;
}
