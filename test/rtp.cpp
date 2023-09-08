#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rtp.h"

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
    uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker,
    uint16_t seq, uint32_t timestamp, uint32_t ssrc)
{
    rtpPacket->rtpHeader.csrcLen = csrcLen;
    rtpPacket->rtpHeader.extension = extension;
    rtpPacket->rtpHeader.padding = padding;
    rtpPacket->rtpHeader.version = version;
    rtpPacket->rtpHeader.payloadType = payloadType;
    rtpPacket->rtpHeader.marker = marker;
    rtpPacket->rtpHeader.seq = seq;
    rtpPacket->rtpHeader.timestamp = timestamp;
    rtpPacket->rtpHeader.ssrc = ssrc;
}


// RTP是采用网络字节序（大端模式），所以要将主机字节字节序转换为网络字节序
int rtpSendPacketOverUdp(int serverRtpSockfd, const char* ip, int16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize) {
    
    struct sockaddr_in addr;
    int ret;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);// 使用htons函数将主机字节序转换为网络字节序
    addr.sin_addr.s_addr = inet_addr(ip);// 将IP地址表示转换为网络字节序

    // 将RTP数据包的头部字段从主机字节序转换为网络字节序，以便在网络上正确传输数据
    // host to net
    rtpPacket->rtpHeader.seq  = htons(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = htonl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = htonl(rtpPacket->rtpHeader.ssrc);

    // size of send data
    ret = sendto(serverRtpSockfd, (char*) rtpPacket, dataSize+RTP_HEADER_SIZE, 0, (sockaddr*)&addr, sizeof(addr));

    // net to host
    rtpPacket->rtpHeader.seq  = ntohs(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = ntohl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = ntohl(rtpPacket->rtpHeader.ssrc);
    return ret; // nums of send bytes
}

// send pkt
