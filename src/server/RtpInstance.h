#ifndef _RTPINSTANNCE_H_
#define _RTPINSTANNCE_H_
#include <string>
#include <stdint.h>
#include <unistd.h>

#include "InetAddress.h"
#include "SocketsOps.h"
#include "Rtp.h"
#include "base/New.h"


// 发送RTP数据包，可以通过UDP或TCP发送
class RtpInstance
{
public:
    enum RtpType
    {
        RTP_OVER_UDP,
        RTP_OVER_TCP
    };

    // 创建一个处理UDP RTP数据流的实例
    static RtpInstance* createNewOverUdp(int localSockfd, uint16_t localPort,
                                    std::string destIp, uint16_t destPort)
    {
        //return new RtpInstance(localSockfd, localPort, destIp, destPort);
        return New<RtpInstance>::allocate(localSockfd, localPort, destIp, destPort);
    }
    // 创建一个处理TCP RTP数据流的实例
    static RtpInstance* createNewOverTcp(int clientSockfd, uint8_t rtpChannel)
    {
        //return new RtpInstance(clientSockfd, rtpChannel);
        return New<RtpInstance>::allocate(clientSockfd, rtpChannel);
    }


    // 多态设计，通过两个不同的构造函数来创建UDP和TCP类型的实例。适应不同的数据传输方式。

    // 构造函数，用于处理UDP RTP数据流
    RtpInstance(int localSockfd, uint16_t localPort, const std::string& destIp, uint16_t destPort) :
        mRtpType(RTP_OVER_UDP), mSockfd(localSockfd), mLocalPort(localPort),
        mDestAddr(destIp, destPort), mIsAlive(false), mSessionId(0) {}

    // 构造函数，用于处理TCP RTP数据流
    RtpInstance(int clientSockfd, uint8_t rtpChannel) :
        mRtpType(RTP_OVER_TCP), mSockfd(clientSockfd), 
        mIsAlive(false), mSessionId(0), mRtpChannel(rtpChannel){}

    ~RtpInstance() { 
        sockets::close(mSockfd);
    }
    
    // 获取本地端口号
    uint16_t getLocalPort() const { return mLocalPort; }
    // 获取远程端口号
    uint16_t getPeerPort() { return mDestAddr.getPort(); }

    // 发送RTP数据包
    int send(RtpPacket* rtpPacket)
    {
        if(mRtpType == RTP_OVER_UDP) {
            return sendOverUdp(rtpPacket->mBuffer, rtpPacket->mSize);
        }else{
            // tcp要加4字节
            uint8_t* rtpPktPtr = rtpPacket->_mBuffer;
            rtpPktPtr[0] = '$';
            rtpPktPtr[1] = (uint8_t)mRtpChannel;
            rtpPktPtr[2] = (uint8_t)(((rtpPacket->mSize)&0xFF00)>>8);
            rtpPktPtr[3] = (uint8_t)((rtpPacket->mSize)&0xFF);
            return sendOverTcp(rtpPktPtr, rtpPacket->mSize+4);
        }
    }
    // 检查连接是否存活
    bool alive() const { return mIsAlive; }
    void setAlive(bool alive) { mIsAlive = alive;}// 设置连接是否存活
    void setSessionId(uint16_t sessionId) { mSessionId = sessionId; }// 设置会话ID
    uint16_t sessionId() const { return mSessionId; }// 获取会话ID

private:
     // 发送UDP/TCP数据包
    int sendOverUdp(void* buf, int size)
    {
        return sockets::sendto(mSockfd, buf, size, mDestAddr.getAddr());
    }

    int sendOverTcp(void* buf, int size)
    {
        return sockets::write(mSockfd, buf, size);
    }

    RtpType mRtpType;
    int mSockfd;
    uint16_t mLocalPort; //for udp
    Ipv4Address mDestAddr; //for udp
    bool mIsAlive;
    uint16_t mSessionId;
    uint8_t mRtpChannel; //for tcp
};

// 发送RTCP数据包。
class RtcpInstance {
public:
    static RtcpInstance* createNew(int localSockfd, uint16_t localPort,
                                    std::string destIp, uint16_t destPort)
    {
        //return new RtcpInstance(localSockfd, localPort, destIp, destPort);
        return New<RtcpInstance>::allocate(localSockfd, localPort, destIp, destPort);
    }

    ~RtcpInstance()
    {
        sockets::close(mLocalSockfd);
    }

    int send(void* buf, int size){
        return sockets::sendto(mLocalSockfd, buf, size, mDestAddr.getAddr());
    }

    int recv(void* buf, int size, Ipv4Address* addr)
    {
        return 0;
    }

    uint16_t getLocalPort() const { return mLocalPort; }

    int alive() const { return mIsAlive; }
    void setAlive(bool alive) { mIsAlive = alive; }
    void setSessionId(uint16_t sessionId) { mSessionId = sessionId; }
    uint16_t sessionId() const { return mSessionId; }

public:
    RtcpInstance(int localSockfd, uint16_t localPort,
                    std::string destIp, uint16_t destPort) :
        mLocalSockfd(localSockfd), mLocalPort(localPort), mDestAddr(destIp, destPort),
        mIsAlive(false), mSessionId(0){}

private:
    int mLocalSockfd;
    uint16_t mLocalPort;
    Ipv4Address mDestAddr;
    bool mIsAlive;
    uint16_t mSessionId;
};

#endif //_RTPINSTANNCE_H_