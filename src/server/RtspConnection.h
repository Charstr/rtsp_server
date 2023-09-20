#ifndef _RTSP_CONNECTION_
#define _RTSP_CONNECTION_
#include <map>

#include "TcpConnection.h"
//#include "net/RtspServer.h"
#include "RtpInstance.h"
#include "media/MediaSession.h"

class RtspServer;

/*
RtspConnection类继承自TcpConnection类，用于处理RTSP连接。它包括处理RTSP请求的方法、处理RTSP命令的方法、处理RTP流的方法等。

*/
class RtspConnection : public TcpConnection
{
public:
    enum Method
    {
        OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER, RTCP,
        NONE,
    };

    static RtspConnection* createNew(RtspServer* rtspServer, int sockfd);
    
    RtspConnection(RtspServer* rtspServer, int sockfd);
    ~RtspConnection();

protected:
    // 处理接收到的字节流的主要入口，根据RTSP请求的不同类型（OPTIONS、DESCRIBE、SETUP、PLAY、TEARDOWN、GET_PARAMETER）分派到不同的处理方法。
    virtual void handleReadBytes();

private:
    // parseRequest用于解析RTSP请求
    bool parseRequest();
    // 第一部分是解析第一行
    bool parseRequest1(const char* begin, const char* end);
    // 第二部分是解析剩余的内容，包括CSeq、Accept、Transport、媒体Track、SessionId等字段。
    bool parseRequest2(const char* begin, const char* end);

    // 解析RTSP请求中的不同字段的辅助方法。
    bool parseCSeq(std::string& message);
    bool parseAccept(std::string& message);
    bool parseTransport(std::string& message);
    bool parseMediaTrack();
    bool parseSessionId(std::string& message);

    // 分别处理不同类型的RTSP命令，并根据具体的需求发送响应。
    bool handleCmdOption();
    bool handleCmdDescribe();
    bool handleCmdSetup();
    bool handleCmdPlay();
    bool handleCmdTeardown();
    bool handleCmdGetParamter();

    // 向客户端发送数据，包括RTSP响应、RTP数据等。
    int sendMessage(void* buf, int size);
    int sendMessage();

    // 创建RTP和RTCP的UDP连接，用于传输媒体数据。
    bool createRtpRtcpOverUdp(MediaSession::TrackId trackId, std::string peerIp,
                        uint16_t peerRtpPort, uint16_t peerRtcpPort);
    //创建RTP over TCP连接，用于传输媒体数据。
    bool createRtpOverTcp(MediaSession::TrackId trackId, int sockfd, uint8_t rtpChannel);
    // 处理RTP over TCP数据，根据通道号将数据分派给相应的RTP实例进行处理。
    void handleRtpOverTcp();

private:
    RtspServer* mRtspServer;
    std::string mPeerIp;
    Method mMethod;
    std::string mUrl;
    std::string mSuffix;
    uint32_t mCSeq;
    uint16_t mPeerRtpPort;
    uint16_t mPeerRtcpPort;
    MediaSession::TrackId mTrackId;
    RtpInstance* mRtpInstances[MEDIA_MAX_TRACK_NUM];
    RtcpInstance* mRtcpInstances[MEDIA_MAX_TRACK_NUM];
    MediaSession* mSession;
    int mSessionId;
    bool mIsRtpOverTcp;
    uint8_t mRtpChannel;
};

#endif //_RTSP_CONNECTION_