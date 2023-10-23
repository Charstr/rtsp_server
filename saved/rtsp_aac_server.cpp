#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/types/FILE.h>
#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <string>

#include "rtp/rtp.h"

#define SERVER_PORT     8554
#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533
#define BUF_MAX_SIZE    (1024*1024)

#define MAX_READ_SIZE 8192
const std::string filename = "/home/more/proj/rtsp_server/example/test.aac";

struct AdtsHeader {
    unsigned int syncword;  //12 bit 同步字 '1111 1111 1111'，一个ADTS帧的开始
    uint8_t id;        //1 bit 0代表MPEG-4, 1代表MPEG-2。
    uint8_t layer;     //2 bit 必须为0
    uint8_t protectionAbsent;  //1 bit 1代表没有CRC，0代表有CRC, 需要额外2字节保存CRC校验码
    
    uint8_t profile;           //1 bit AAC级别（MPEG-2 AAC中定义了3种profile，MPEG-4 AAC中定义了6种profile）
    uint8_t samplingFreqIndex; //4 bit 采样率
    uint8_t privateBit;        //1bit 编码时设置为0，解码时忽略
    uint8_t channelCfg;        //3 bit 声道数量
    uint8_t originalCopy;      //1bit 编码时设置为0，解码时忽略
    uint8_t home;               //1 bit 编码时设置为0，解码时忽略

    uint8_t copyrightIdentificationBit;   //1 bit 编码时设置为0，解码时忽略
    uint8_t copyrightIdentificationStart; //1 bit 编码时设置为0，解码时忽略
    unsigned int aacFrameLength;               //13 bit 一个ADTS帧的长度包括ADTS头和AAC原始流
    unsigned int adtsBufferFullness;           //11 bit 缓冲区充满度，0x7FF说明是码率可变的码流，不需要此字段。CBR可能需要此字段，不同编码器使用情况不同。这个在使用音频编码的时候需要注意。

    /* number_of_raw_data_blocks_in_frame
     * 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧
     * 所以说number_of_raw_data_blocks_in_frame == 0
     * 表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
     */
    uint8_t numberOfRawDataBlockInFrame; //2 bit
};

static int createTcpSocket(){
    int sockfd;
    int on = 1;
    // tcp
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int createUdpSocket() {
    int sockfd;
    int on = 1;
    // udp
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int bindSocketAddr(int sockfd, const char* ip, int port)
{
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
        return -1;

    return 0;
}
  
static int acceptClient(int sockfd, char* ip, int* port) {
    int clientfd;
    socklen_t len = 0;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);
    // 
    clientfd = accept(sockfd, (struct sockaddr*)&addr, &len);
    if (clientfd < 0)  return -1;

    strcpy(ip, inet_ntoa(addr.sin_addr));
    *port = ntohs(addr.sin_port);

    return clientfd;
}

/*-------------RTSP-------------*/

static int handleCmd_OPTIONS(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
        "\r\n",
        cseq);

    return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, char* url) {
    char sdp[500];
    char localIp[100];

    sscanf(url, "rtsp://%[^:]:", localIp);

    sprintf(sdp, "v=0\r\n"
        "o=- 9%ld 1 IN IP4 %s\r\n"
        "t=0 0\r\n"
        "a=control:*\r\n"
        "m=audio 0 RTP/AVP 97\r\n"
        "a=rtpmap:97 mpeg4-generic/44100/2\r\n"
        "a=fmtp:97 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1210;\r\n"
   
        //"a=fmtp:97 SizeLength=13;\r\n"
        "a=control:track0\r\n",
        time(NULL), localIp);

    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
        "Content-Base: %s\r\n"
        "Content-type: application/sdp\r\n"
        "Content-length: %zu\r\n\r\n"
        "%s",
        cseq,
        url,
        strlen(sdp),
        sdp);

    return 0;
}

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
        "Session: 66334873\r\n"
        "\r\n",
        cseq,
        clientRtpPort,
        clientRtpPort + 1,
        SERVER_RTP_PORT,
        SERVER_RTCP_PORT
    );

    return 0;
}

static int handleCmd_PLAY(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Range: npt=0.000-\r\n"
        "Session: 66334873; timeout=10\r\n\r\n",
        cseq);

    return 0;
}

/*-----------------RTSP------------*/

static char* getLineFromBuf(char* buf, char* line) {
    while(*buf != '\n'){
        *line++ = *buf++;
    }
    *line++ = '\n';
    *line = '\0';
    return ++buf; 
}

// 解析读出来的7个字节到adts的head
static int parseAdtsHeader(uint8_t* in, struct AdtsHeader* res) {
    static int frame_number = 0;
    memset(res, 0, sizeof(*res));

    if ((in[0] == 0xFF) && ((in[1] & 0xF0) == 0xF0)) {
        res->id = ((uint8_t)in[1] & 0x08) >> 3;//第二个字节与0x08与运算之后，获得第13位bit对应的值
        res->layer = ((uint8_t)in[1] & 0x06) >> 1;//第二个字节与0x06与运算之后，右移1位，获得第14,15位两个bit对应的值
        res->protectionAbsent = (uint8_t)in[1] & 0x01;
        res->profile = ((uint8_t)in[2] & 0xc0) >> 6;
        res->samplingFreqIndex = ((uint8_t)in[2] & 0x3c) >> 2;
        res->privateBit = ((uint8_t)in[2] & 0x02) >> 1;
        res->channelCfg = ((((uint8_t)in[2] & 0x01) << 2) | (((unsigned int)in[3] & 0xc0) >> 6));
        res->originalCopy = ((uint8_t)in[3] & 0x20) >> 5;
        res->home = ((uint8_t)in[3] & 0x10) >> 4;
        res->copyrightIdentificationBit = ((uint8_t)in[3] & 0x08) >> 3;
        res->copyrightIdentificationStart = (uint8_t)in[3] & 0x04 >> 2;
        
        res->aacFrameLength = (((((unsigned int)in[3]) & 0x03) << 11) |
            (((unsigned int)in[4] & 0xFF) << 3) |
            ((unsigned int)in[5] & 0xE0) >> 5);

        res->adtsBufferFullness = (((unsigned int)in[5] & 0x1f) << 6 |
            ((unsigned int)in[6] & 0xfc) >> 2);
        res->numberOfRawDataBlockInFrame = ((uint8_t)in[6] & 0x03);
        return 0;
    }else{
        printf("failed to parse adts header\n");
        return -1;
    }
}

static void rtpSendAACFrame(int socket, const char* ip, int16_t port,
    struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize) {
    int ret;

    rtpPacket->payload[0] = 0x00;
    rtpPacket->payload[1] = 0x10;
    rtpPacket->payload[2] = (frameSize & 0x1FE0) >> 5; //高8位
    rtpPacket->payload[3] = (frameSize & 0x1F) << 3; //低5位

    memcpy(rtpPacket->payload + 4, frame, frameSize);

    ret = rtpSendPacketOverUdp(socket, ip, port, rtpPacket, frameSize + 4);
    if (ret < 0){
        printf("failed to send rtp packet\n");
        return;
    }

    rtpPacket->rtpHeader.seq++;

    /*
     * 如果采样频率是44100
     * 一般AAC每个1024个采样为一帧
     * 所以一秒就有 44100 / 1024 = 43帧
     * 时间增量就是 44100 / 43 = 1025
     * 一帧的时间为 1 / 43 = 23ms
     */
    rtpPacket->rtpHeader.timestamp += 1025;
}

static void doClient(int clientSockfd, const char* clientIP, int clientPort,
                         const std::string &filename) {

    char method[40];
    char url[100];
    char version[40];
    int cseq;
    int serverRtpSockfd , serverRtcpSockfd;
    int clientRtpPort, clientRtcpPort;
    char *bufPtr;
    char* recvBuf = (char*)malloc(BUF_MAX_SIZE); // query
    char* rspBuf = (char*)malloc(BUF_MAX_SIZE); // answer
    char line[400];// 消息是一行一行的，每行结尾一个\r\n，line是读取出来一行

    while(true) {
        int recvLen;
        // receive from client
        recvLen = recv(clientSockfd, recvBuf, BUF_MAX_SIZE, 0);
        if(recvLen <= 0) break;
        // 接收客户端发送的消息对消息进行解析
        // recvBuf客户端发送的完整消息
        recvBuf[recvLen] = '\0';
        printf("---------------C->S--------------\n");
        printf("%s", recvBuf);
 
        /* 解析方法*/
        // 读取一行\n到line中,并将指针移动到\n之后
        bufPtr = getLineFromBuf(recvBuf, line);
        if(sscanf(line, "%s %s %s\r\n", method, url, version) != 3) {
            printf("parse method err\n");
            break;
        }
        /* 解析序列号 */
        bufPtr = getLineFromBuf(bufPtr, line);
        if(sscanf(line, "CSeq: %d\r\n", &cseq) != 1) {
            printf("parse CSeq err\n");
            break;
        }
        /* 如果是SETUP，transport字段有client_port,提取出客户端的rtp和rtcp端口*/
        // 0 means equal
        if(!strcmp(method, "SETUP")){
            while(1) {
                bufPtr = getLineFromBuf(bufPtr, line);
                if(!strncmp(line, "Transport:", strlen("Transport:"))){
                    sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
                                    &clientRtpPort, &clientRtcpPort);
                    break;
                }
            }
        }

        // 每次客户端的请求都会有method url version，seq等信息
        // 根据method和CSeq形成对应的回应rspBuf，通过发送给客户端的socket

        // 根据method和cSeq进行相应的回应，拼接成rspBuf

        if(!strcmp(method, "OPTIONS")){
            if(handleCmd_OPTIONS(rspBuf, cseq)){
                printf("failed to handle options\n");
                break;
            }
        }
        else if(!strcmp(method, "DESCRIBE")){
            if(handleCmd_DESCRIBE(rspBuf, cseq, url)){
                printf("failed to handle describe\n");
                break;
            }
        }
        else if(!strcmp(method, "SETUP")) {

            if(handleCmd_SETUP(rspBuf, cseq, clientRtpPort))
            {
                printf("failed to handle setup\n");
                break;
            }

            serverRtpSockfd = createUdpSocket();
            serverRtcpSockfd = createUdpSocket();
            if(serverRtpSockfd < 0 || serverRtcpSockfd < 0){
                printf("failed to create udp socket\n");
                return;
            }

            if(bindSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT) < 0 ||
                bindSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT) < 0){
                printf("udp failed to bind addr\n");
                return;
            }
        }
        else if(!strcmp(method, "PLAY")){
            if(handleCmd_PLAY(rspBuf, cseq))
            {
                printf("failed to handle play\n");
                break;
            }
        } else {
            printf("undefined method = %s \n", method);
            break;
        }

        // 每一次接收到客户端的命令，服务端进行回复
        printf("---------------S->C--------------\n");
        printf("%s", rspBuf);
        // 服务端回复客户端
        send(clientSockfd, rspBuf, strlen(rspBuf), 0);
        

        //  play方法的时候，服务端开始rtsp发送数据
        if(!strcmp(method, "PLAY")){
            // 发送的一帧
            uint8_t *frame = (uint8_t*)malloc(MAX_READ_SIZE);
            // 发送的rtp包
            RtpPacket *rtpPacket = (RtpPacket*) malloc(MAX_READ_SIZE);
            if(!rtpPacket || !frame){
                printf("allocate memory err\n");
                break;
            }
            FILE *fp = fopen(filename.c_str(), "rb");
            if(!fp){
                printf("failed to open file\n");
                break;
            }

            struct AdtsHeader adtsHeader;
            
            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_AAC, 
                            1, 0, 0, 0x32411);
            int ret;
            while(true){
                // 先读取aac的头部7字节。
                ret = fread(frame, 1, 7, fp);
                if (ret <= 0){
                    printf("fread err\n");
                    break;
                }
                // 解析头部，读出来头部
                if (parseAdtsHeader(frame, &adtsHeader) < 0){
                    printf("parseAdtsHeader err\n");
                    break;
                }
                ret = fread(frame, 1, adtsHeader.aacFrameLength - 7, fp);
                if (ret <= 0){
                    printf("fread err\n");
                    break;
                }
                rtpSendAACFrame(serverRtpSockfd, clientIP, clientRtpPort,
                    rtpPacket, frame, adtsHeader.aacFrameLength - 7);
                usleep(23223);
            } 
            free(frame);
            free(rtpPacket);
            break;
        }
    }
    if(serverRtpSockfd) close(serverRtpSockfd);
    if(serverRtcpSockfd) close(serverRtcpSockfd);

    free(rspBuf);
    free(recvBuf);
    // accept
}
int main(int argc, char* argv[]){

    // send
    int serverSocketfd = createTcpSocket();
    if(serverSocketfd<0){
        printf("create tcp socket failed\n");
        return -1;
    }
    if(bindSocketAddr(serverSocketfd, "0.0.0.0",  SERVER_PORT)){
        printf("bind tcp server failed\n");
        return -1;
    }
    if(listen(serverSocketfd, 10)<0){
        printf("failed to listen\n");
        return -1;
    }
    printf("rtsp://127.0.0.1:%d\n", SERVER_PORT);
    while(true){
        int clientSocketfd;
        char clientIp[40];
        int clientPort;
        // 获得客户端的IP以及对应的端口
        clientSocketfd = acceptClient(serverSocketfd, clientIp, &clientPort);
        if(clientSocketfd<0){
            printf("failed to accept client\n");
            return -1;
        }
        printf("accept client;client ip:%s,client port:%d\n", clientIp, clientPort);
        doClient(clientSocketfd, clientIp, clientPort, filename);
        close(clientSocketfd);
    }
    close(serverSocketfd);
    return 0;
}