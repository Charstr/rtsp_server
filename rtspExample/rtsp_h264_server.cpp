#include <arpa/inet.h>
#include <asm-generic/socket.h>
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
#define MAX_READ_SIZE 524288

static int createTcpSocket()
{
    int sockfd;
    int on = 1;
    // TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int createUdpSocket()
{
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
    addr.sin_port = htons(port); // host to net
    addr.sin_addr.s_addr = inet_addr(ip);//显式指定一个IP地址

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0)
        return -1;

    return 0;
}
  
static int acceptClient(int sockfd, char* ip, int* port)
{
    int clientfd;
    socklen_t len = 0;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);

    clientfd = accept(sockfd, (struct sockaddr*)&addr, &len);
    if (clientfd < 0)
        return -1;

    strcpy(ip, inet_ntoa(addr.sin_addr));
    *port = ntohs(addr.sin_port);

    return clientfd;
}

static inline bool startCode3(char* buf) {
    return buf[0] == 0 && buf[1] == 0 && buf[2] == 1;
}

static inline bool startCode4(char* buf) {
    return buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1;
}

static char* findNextStartCode(char* buf, int len)
{
    int i;

    if (len < 3)
        return nullptr;
    // 下一个开始标志位
    for (i = 0; i < len - 3; ++i)
    {
        if (startCode3(buf) || startCode4(buf))
            return buf;

        ++buf;
    }

    if (startCode3(buf))
        return buf;

    return nullptr;
}


// 读取数据到frame，一次一个nalu
static int getFrameFromH264File(FILE* fp, char* frame, int size) {
    int rSize, frameSize;
    char* nextStartCode;
    if (!fp) return -1;

    // 从文件流fp中读取指定大小的数据到frame中，读取完成后，文件流fp会自动向后偏移size个字节的位置
    // rSize实际读取了多少个字节.如果rSize等于size，则表示成功读取了所需数量的字节。
    // 如果rSize小于size，则可能已经达到了文件的末尾或发生了读取错误。

    rSize = fread(frame, 1, size, fp);

    if (!startCode3(frame) && !startCode4(frame))
        return -1;

    nextStartCode = findNextStartCode(frame + 3, rSize - 3);
    if(!nextStartCode) return -1;   
    else{
        frameSize = (nextStartCode - frame);
        // rSize是读取的大小，然后fp偏移fSize大小。接下来读取了一个nalu，
        // 这个nalu比rSize小，所以要将fp偏移到一个nalu的实际位置
        // frameSize - rSize小于0，也就是向前偏移到实际的一个nalu的结尾
        fseek(fp, frameSize - rSize, SEEK_CUR);
    }
    return frameSize; // size of one nalu
}

static void rtpSendH264Frame(int serverRtpSockfd, const char* ip, int16_t port,
    struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize) {

    uint8_t naluType; // nalu第一个字节
    int ret;
    naluType = frame[0];
    // nalu长度小于最大包场：单一NALU单元模式
    if (frameSize <= RTP_MAX_PKT_SIZE){
        /*
         * low 5 bits is type
         *   0               1                   2
         *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6...
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |F|NRI|  Type   | a single NAL unit ... |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        memcpy(rtpPacket->payload, frame, frameSize);
        ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, frameSize);
        if(ret < 0) return;

        rtpPacket->rtpHeader.seq++;
        // 如果是SPS、PPS就不需要加时间戳,直接返回 sendBytes
         if ((naluType & 0x1F) == 7 || (naluType & 0x1F) == 8) return;
    }else{// nalu长度小于最大包：分片模式
        /*
         *  0                   1                   2
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * | FU indicator  |   FU header   |   FU payload   ...  |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        /*
         *     FU Indicator
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |F|NRI|  Type   |
         *   +---------------+
         * 第一个字节 FU Indicator，高三位和nalu的相同，低5位type为28表示这个nalu分片了
         */

        /*
         *      FU Header
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |S|E|R|  Type   |
         *   +---------------+
         */
        int pktNum = frameSize / RTP_MAX_PKT_SIZE;       // 有几个完整的包
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE; // 剩余不完整包的大小
        int i, pos = 1;
        /* 发送完整的包 */
        for (i = 0; i < pktNum; i++) {
            // 第一个字节高三位和nalu的相同，低5位type为28
            // 高一位规范位必须为0，所以不用管吗，直接设置2-3位即可
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            // 第二个字节nalu type
            rtpPacket->payload[1] = naluType & 0x1F;
            if (i == 0) //第一包数据
                rtpPacket->payload[1] |= 0x80; // start
            else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据,没有不完整包
                rtpPacket->payload[1] |= 0x40; // end
            
            // pos=1,分块的时候不需要发送nalu的head，已经在rtp包头前边的两个字节
            memcpy(rtpPacket->payload+2, frame+pos, RTP_MAX_PKT_SIZE);
            // 2bits的标志位+ RTP_MAX_PKT_SIZE data
            ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, RTP_MAX_PKT_SIZE+2);
            if(ret < 0) return;
            rtpPacket->rtpHeader.seq++;
            pos += RTP_MAX_PKT_SIZE;
        }
        /* 发送剩余的数据 */
        if (remainPktSize > 0) {
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;
            rtpPacket->payload[1] |= 0x40; //end
            // +2 ?
            memcpy(rtpPacket->payload+2, frame+pos, remainPktSize);
            // 发送的数据应该是2字节的标志位+剩余包的大小
            ret = rtpSendPacketOverUdp(serverRtpSockfd, ip, port, rtpPacket, remainPktSize+2);
            if(ret < 0) return;
            rtpPacket->rtpHeader.seq++;
        }
    }
    // 如果是SPS、PPS就不需要加时间戳

    rtpPacket->rtpHeader.timestamp+=90000/25;
}

/*-----------------RTSP--------------*/

static int handleCmd_OPTIONS(char* result, int cseq){
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
    sscanf(url, "rtsp://%[^:]:", localIp);// regex get IP
    // video content
    sprintf(sdp, "v=0\r\n"
                 "o=- 9%ld 1 IN IP4 %s\r\n"
                 "t=0 0\r\n"
                 "a=control:*\r\n"
                 "m=video 0 RTP/AVP 96\r\n"
                 "a=rtpmap:96 H264/90000\r\n"
                 "a=control:track0\r\n",
                 time(nullptr), localIp);
    
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

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort){
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
                    "Session: 66334873\r\n"
                    "\r\n",
                    cseq,
                    clientRtpPort,
                    clientRtpPort+1,
                    SERVER_RTP_PORT,
                    SERVER_RTCP_PORT);
    
    return 0;
}

static int handleCmd_PLAY(char* result, int cseq) {
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Range: npt=0.000-\r\n"
                    "Session: 66334873; timeout=60\r\n\r\n",
                    cseq);
    
    return 0;
}


static char* getLineFromBuf(char* buf, char* line) {
    while(*buf != '\n') {
        *line++ = *buf++;
    }
    *line++ = '\n';
    *line = '\0';
    return ++buf; 
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
    char line[400];

    while(true) {
        int recvLen;
        // receive from client
        recvLen = recv(clientSockfd, recvBuf, BUF_MAX_SIZE, 0);
        if(recvLen <= 0) break;

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

        /* 如果是SETUP，那么就再解析client_port,提取出客户端的rtp和rtcp端口*/
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
        }
        // teardown...
        else {
            printf("undefined method = %s \n", method);
            break;
        }

        printf("---------------S->C--------------\n");
        printf("%s", rspBuf);
        // send to client
        send(clientSockfd, rspBuf, strlen(rspBuf), 0);
        
        /* 开始播放，发送RTP包 */
        if(!strcmp(method, "PLAY")){
            int startCode, frameSize;

            RtpPacket *rtpPacket = (RtpPacket*)malloc(MAX_READ_SIZE);
            char *frame = (char*)malloc(MAX_READ_SIZE); // cache tmp frame

            if(!rtpPacket || !frame){
                printf("allocate memory err\n");
                break;
            }

            FILE* fp = fopen(filename.c_str(), "rb");
            if(!fp){
                printf("failed to open %s\n", filename.c_str());
                return;
            }

            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VERSION, RTP_PAYLOAD_TYPE_H264, 0,
                            0, 0, 0x88923423);
            printf("start play\n");
            printf("client ip:%s\n", clientIP);
            printf("client port:%d\n", clientRtpPort);

            while(true){
                // frameSize是包含有startCode的
                frameSize = getFrameFromH264File(fp, frame, MAX_READ_SIZE);
                //printf("framesize: %d\n", frameSize);
                if(frameSize < 0) {
                    printf("end of file\n");
                    break;
                }
                startCode = startCode3(frame) ? 3 : 4;

                // a nalu size
                frameSize-=startCode;
                // send frame
                rtpSendH264Frame(serverRtpSockfd, clientIP, clientRtpPort,
                                    rtpPacket, frame+startCode, frameSize);

                //sleep(40);
                usleep(1000*1000/25);
            }
            fclose(fp);
            free(rtpPacket);
            free(frame);
        }
        memset(recvBuf, 0, sizeof(*recvBuf));
        memset(rspBuf, 0, sizeof(*rspBuf));
    }

    close(clientSockfd);
    if(serverRtpSockfd) close(serverRtpSockfd);
    if(serverRtcpSockfd) close(serverRtcpSockfd);
    free(recvBuf);
    free(rspBuf);
}


int main(int argc, char *argv[]){
    // input file name
    std::string filename = "/home/more/proj/myRTSP/test/test.h264";

    int serverSockfd;
    int serverRtpSockfd, serverRtcpSockfd;
    int ret;

    serverSockfd = createTcpSocket();
    if(serverSockfd < 0){
        printf("failed to create tcp socket\n");
        return -1;
    }

    ret = bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT);
    if(ret < 0)
    {
        printf("tcp failed to bind addr\n");
        return -1;
    }
    // listen后该socket变成被动套接字,默认主动
    // 被动套接字用来接受连接,会调用accept函数
    // 主动套接字用来发起连接,会调用connect函数
    ret = listen(serverSockfd, 10);

    if(ret < 0)
    {
        printf("failed to listen\n");
        return -1;
    }

    printf("rtsp://127.0.0.1:%d\n", SERVER_PORT);

    while(true){
        int clientSockfd;
        char clientIp[40];
        int clientPort;
        
        // get client IP and port
        clientSockfd = acceptClient(serverSockfd, clientIp, &clientPort);
        if(clientSockfd < 0){
            printf("failed to accept client\n");
            return -1;
        }

        printf("accept client;client ip:%s,client port:%d\n", clientIp, clientPort);

        doClient(clientSockfd, clientIp, clientPort, filename);
    }

    return 0;
}