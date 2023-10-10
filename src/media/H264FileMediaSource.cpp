#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "H264FileMediaSource.h"
#include "base/Logging.h"
#include "base/New.h"

static inline bool startCode3(uint8_t* buf);
static inline bool startCode4(uint8_t* buf);

H264FileMediaSource* H264FileMediaSource::createNew(UsageEnvironment* env, std::string file)
{
    return New<H264FileMediaSource>::allocate(env, file);
}

// 打开文件，设置视频帧率，
H264FileMediaSource::H264FileMediaSource(UsageEnvironment* env, const std::string& file) :
    MediaSource(env),
    mFile(file)
{
    // MediaSource创建初始化一个缓冲区，对应到mAVFrameInputQueue队列，设置线程的任务回调函数MediaSource::taskCallback，通过多态读取调用H264FileMediaSource::readFrame从h264文件读取一个AVFrame到临时缓冲到mAVFrameInputQueue，指定位置是设置的缓冲区mAVFrames，然后取出到mAVFrameOutputQueue
    mFd = ::open(file.c_str(), O_RDONLY);
    assert(mFd > 0);

    // 视频帧率
    setFps(30);

    // 添加任务mTask到线程池的任务队列mTaskQueue，
    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)
        mEnv->threadPool()->addTask(mTask);
    
}

H264FileMediaSource::~H264FileMediaSource(){
    ::close(mFd);
}


// 从h264文件读取一帧
// 生产数据
void H264FileMediaSource::readFrame(){
    MutexLockGuard mutexLockGuard(mMutex);

    if(mAVFrameInputQueue.empty()) return;

    // 取出队首的指针
    AVFrame* frame = mAVFrameInputQueue.front();

    // 生产者从mFd文件描述符读取最大FRAME_MAX_SIZE大小到frame->mBuffer的buf中存储，并返回读取的大小
    frame->mFrameSize = getFrameFromH264File(mFd, frame->mBuffer, FRAME_MAX_SIZE);

    if(frame->mFrameSize < 0) return;

    if(startCode3(frame->mBuffer)){
        frame->mFrame = frame->mBuffer+3;
        frame->mFrameSize -= 3;
    } else {
        frame->mFrame = frame->mBuffer+4;
        frame->mFrameSize -= 4;
    }

    // 循环队列，进行数据生产
    mAVFrameInputQueue.pop();
    // 输出队列由定时器完成回调
    mAVFrameOutputQueue.push(frame);
}

static inline bool startCode3(uint8_t* buf){
    return buf[0] == 0 && buf[1] == 0 && buf[2] == 1;
}

static inline bool startCode4(uint8_t* buf){
    return buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1;
}

static uint8_t* findNextStartCode(uint8_t* buf, int len){

    int i;

    if(len < 3) return nullptr;

    for(i = 0; i < len-3; ++i){
        if(startCode3(buf) || startCode4(buf))
            return buf;
        ++buf;
    }

    if(startCode3(buf)) return buf;

    return nullptr;
}

int H264FileMediaSource::getFrameFromH264File(int fd, uint8_t* frame, int size){
    int rSize, frameSize;
    uint8_t* nextStartCode;

    if(fd < 0) return fd;

    rSize = read(fd, frame, size);
    if(!startCode3(frame) && !startCode4(frame))
        return -1;
    
    nextStartCode = findNextStartCode(frame+3, rSize-3);
    if(!nextStartCode) {
        lseek(fd, 0, SEEK_SET);
        frameSize = rSize;
    } else{
        frameSize = (nextStartCode-frame);
        lseek(fd, frameSize-rSize, SEEK_CUR);
    }

    return frameSize;
}