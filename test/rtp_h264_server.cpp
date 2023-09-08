#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <string>

#include "net/rtp.h"

#define H264_FILE_NAME  "test.h264"
#define CLIENT_IP       "127.0.0.1"
#define CLIENT_PORT     9832
#define MAX_SIZE 524288
#define FPS             25

static int createUdpSocket(){
    int fd;
    int on = 1;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd<0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    return fd;
} 

static inline bool startCode3(char* buf) {
    return buf[0] == 0 && buf[1] == 0 && buf[2] == 1;
}

static inline bool startCode4(char* buf) {
    return buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1;
}

// 

static char* findNextStartCode(char *buf, int len){
    if(len<3) return nullptr;
    // 找开始标志位,
    for(int i=0;i<len-3;i++){
        if(startCode3(buf) || startCode4(buf)) return buf;
        buf++;
    }
    if(startCode3(buf)) return buf;
    return nullptr;
}

// read size to frame
// one nalu
static int getFrameFromH264File(FILE *fp, char *frame, int size){

    int rSize, frameSize;
    char* nextStartCode;
    if (!fp) return -1;

    // 从文件流fp中读取指定大小的数据到frame中，读取完成后，文件流fp会自动向后偏移size个字节的位置
    // rSize实际读取了多少个字节.如果rSize等于size，则表示成功读取了所需数量的字节。
    // 如果rSize小于size，则可能已经达到了文件的末尾或发生了读取错误。

    rSize = fread(frame, 1, size, fp);

    if (!startCode3(frame) && !startCode4(frame)) 
        return -1;
    // get next startcode
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
static int rtpSendH264Frame(int socket, char* ip, int16_t port,
                            struct RtpPacket* rtpPacket, char* frame, uint32_t frameSize){

    uint8_t naluType; // nalu第一个字节
    int sendBytes = 0;
    int ret;
    naluType = frame[0];
    // nalu长度小于最大包场：单一NALU单元模式
    if (frameSize <= RTP_MAX_PKT_SIZE){
        /*
         * low b bits is type
         *   0               1                   2
         *   0 1 2 3 4 5 6 7 0 1 2 3 4 5 6...
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |F|NRI|  Type   | a single NAL unit ... |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        memcpy(rtpPacket->payload, frame, frameSize);
        ret = rtpSendPacketOverUdp(socket, ip, port, rtpPacket, frameSize);
        if(ret < 0)
            return -1;
        rtpPacket->rtpHeader.seq++;
        sendBytes += ret;
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
            else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据
                rtpPacket->payload[1] |= 0x40; // end
            
            // pos=1,分块的时候不需要发送nalu的head，已经在rtp包头前边的两个字节
            memcpy(rtpPacket->payload+2, frame+pos, RTP_MAX_PKT_SIZE);
            ret = rtpSendPacketOverUdp(socket, ip, port, rtpPacket, RTP_MAX_PKT_SIZE+2);
            if(ret < 0) return -1;
            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
            pos += RTP_MAX_PKT_SIZE;
        }
        /* 发送剩余的数据 */
        if (remainPktSize > 0) {
            // 第一个字节高三位和nalu的相同，低5位type为28
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            // 第二个字节nalu type
            rtpPacket->payload[1] = naluType & 0x1F;
            rtpPacket->payload[1] |= 0x40; //end

            memcpy(rtpPacket->payload+2, frame+pos, remainPktSize+2);// +2?
            // left data + 2byte head
            ret = rtpSendPacketOverUdp(socket, ip, port, rtpPacket, remainPktSize);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
        }
    }
    // 如果是SPS、PPS就不需要加时间戳
    if(!((naluType&0x1F)==7 || (naluType&0x1F)==8)) return sendBytes;
    rtpPacket->rtpHeader.timestamp+=90000/25;

    return sendBytes;
}


int main(int argc, char *argv[]){
    int socket, startCode;
    std::string filename = "/home/more/proj/myRTSP/example/test.h264";
    FILE* fp = fopen(filename.c_str(), "rb");
    uint32_t frameSize;
    if(!fp){
        printf("failed to open %s\n", filename.c_str());
        return -1;
    }
    
    socket = createUdpSocket(); // udp
    if(socket<0){
        printf("failed to create udp socket\n");
        return -1;
    }

    RtpPacket *rtpPacket;
    // read a pkt and frame
    rtpPacket = (RtpPacket*)malloc(MAX_SIZE);

    char *frame = (char*)malloc(MAX_SIZE); // cache tmp frame
    if(!rtpPacket || !frame){
        printf("allocate memory err\n");
        return -1;
    }
    rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H264, 0,
                    0, 0, 0x88923423);

    while(1){
        // frameSize是包含有startCode的
        frameSize = getFrameFromH264File(fp, frame, 500000);
        if(frameSize < 0) {
            printf("read err\n");
            continue;
        }

        startCode = startCode3(frame) ? 3 : 4;

        // a nalu size
        frameSize-=startCode;
        // send frame
        rtpSendH264Frame(socket, const_cast<char*>(CLIENT_IP), CLIENT_PORT,
                            rtpPacket, frame+startCode, frameSize);

        usleep(1000*1000/FPS);
    }
    free(rtpPacket);
    free(frame);

    return 0;
}