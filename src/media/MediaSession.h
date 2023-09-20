#ifndef _MEDIASESSION_H_
#define _MEDIASESSION_H_
#include <string>
#include <list>

#include "server/RtpInstance.h"
#include "RtpSink.h"

#define MEDIA_MAX_TRACK_NUM 2

/*
MediaSession类用于创建、管理和控制多媒体会话,允许用户添加多个媒体轨道（Track）。

*/
class MediaSession
{
public:
    enum TrackId
    {
        TrackIdNone = -1,
        TrackId0    = 0,
        TrackId1    = 1,
    };
    // 创建一个新的媒体会话
    static MediaSession* createNew(std::string sessionName);

    MediaSession(const std::string& sessionName);
    ~MediaSession();
    // 获取会话名称
    std::string name() const { return mSessionName; }
    // 生成SDP描述,通常用于媒体会话的初始化和协商,包含会话的基本信息、媒体轨道的描述以及其他会话参数。
    std::string generateSDPDescription();
    // 添加RtpSink用于发送媒体数据
    bool addRtpSink(MediaSession::TrackId trackId, RtpSink* rtpSink);
    // 添加RtpInstance，用于接收媒体数据
    bool addRtpInstance(MediaSession::TrackId trackId, RtpInstance* rtpInstance);
    bool removeRtpInstance(RtpInstance* rtpInstance);
    // 多播
    bool startMulticast();
    bool isStartMulticast();
    std::string getMulticastDestAddr() const { return mMulticastAddr; }
    uint16_t getMulticastDestRtpPort(TrackId trackId);

private:
    // 媒体会话中的单个媒体轨道由Track类表示，包括一个RtpSink（用于发送媒体数据）和一个RtpInstance列表（用于接收媒体数据）
    class Track
    {
    public:
        RtpSink* mRtpSink;// 用于发送媒体数据的RtpSink
        int mTrackId;// 轨道ID
        bool mIsAlive; // 是否活动
        std::list<RtpInstance*> mRtpInstances;// 用于接收媒体数据的RtpInstance列表
    };
    // 获取指定轨道
    Track* getTrack(MediaSession::TrackId trackId);
    // 发送媒体数据的回调函数
    static void sendPacketCallback(void* arg1, void* arg2, RtpPacket* rtpPacket);
    // 发送媒体数据
    void sendPacket(MediaSession::Track* tarck, RtpPacket* rtpPacket);

private:
    std::string mSessionName;// 会话名称
    std::string mSdp;// SDP描述
    Track mTracks[MEDIA_MAX_TRACK_NUM]; // 媒体轨道数组
    bool mIsStartMulticast;
    std::string mMulticastAddr;
    // rtp rtcp
    RtpInstance* mMulticastRtpInstances[MEDIA_MAX_TRACK_NUM];
    RtcpInstance* mMulticastRtcpInstances[MEDIA_MAX_TRACK_NUM];
};

#endif //_MEDIASESSION_H_