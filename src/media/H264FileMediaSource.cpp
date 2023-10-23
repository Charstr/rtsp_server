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
    // MediaSource构造函数设置了线程任务mTask回调函数MediaSource::taskCallback，

    // MediaSource创建初始化一个缓冲区，对应到mAVFrameInputQueue队列，通过多态读取调用H264FileMediaSource::readFrame从h264文件读取一个AVFrame到临时缓冲到mAVFrameInputQueue，指定位置是设置的缓冲区mAVFrames，然后取出到mAVFrameOutputQueue
    mFd = ::open(file.c_str(), O_RDONLY);
    assert(mFd > 0);

    setFps(30);

    /*

    线程池的任务：
    1. 将mTask插入到任务队列mTaskQueue，并Condition->signal();唤醒一个等待的线程进行处理。

    1. 添加到线程池的任务队列，创建线程的时候设置有处理任务的函数 Thread::threadRun，通过多态调用ThreadPool::MThread::run处理任务
    2. 调用线程池的ThreadPool::handleTask函数，从任务队列mTaskQueue取出来任务并调用对应任务的回调函数（这里设置的是MediaSource::taskCallback，通过多态调用H264FileMediaSource::readFrame从h264文件读取一个AVFrame到mAVFrameOutputQueue）执行任务
    3. 每个任务通过回调会读取一个视频帧数据并添加到队列mAVFrameOutputQueue
    */


    // 当提交了一个任务后，通过条件变量 mCondition->signal()唤醒其中一个空闲线程，被唤醒的线程会从任务队列 mTaskQueue 中取出任务，然后执行该任务的回调函数。代码中的是Thread::threadRun，多态调用ThreadPool::MThread::run，函数内部调用 ThreadPool::handleTask 函数，执行具体的任务逻辑。

    // 线程池处理任务是异步执行的，任务的提交和执行是分离的，任务在一个线程池中排队等待执行，而不是立即在提交的地方执行。程序可以继续执行后续的逻辑，而不必等待任务完成。
    // 异步执行的方式可以提高程序的并发性和响应性，特别是在处理I/O密集型任务（比如文件读写、网络通信等）时，可以充分利用CPU资源，提高程序的吞吐量。

    // 这里为什么添加mTask到任务队列mTaskQueue？避免了线程池在启动后还需要一定时间才能接受到新的任务，提高系统的响应速度
    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)
        mEnv->threadPool()->addTask(mTask);
}

H264FileMediaSource::~H264FileMediaSource(){
    ::close(mFd);
}


// 从h264文件读取一帧，线程池任务队列的多态调用
void H264FileMediaSource::readFrame(){
    MutexLockGuard mutexLockGuard(mMutex);

    if(mAVFrameInputQueue.empty()) return;

    // 取出队首的指针
    AVFrame* frame = mAVFrameInputQueue.front();

    // 生产者从mFd文件描述符读取最大FRAME_MAX_SIZE大小到frame->mBuffer的buf中存储，并返回读取的大小
    // mFd确定了文件要读取的位置
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