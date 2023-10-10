#include "MediaSource.h"
#include "base/Logging.h"
#include "base/New.h"
#include <cstddef>

MediaSource::MediaSource(UsageEnvironment* env) :
    mEnv(env)
{
    mMutex = Mutex::createNew();

    // 初始化视频帧缓冲池mAVFrames，缓冲区大小为DEFAULT_FRAME_NUM
    // 将mAVFrameInputQueue跟缓冲区对应
    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)
        mAVFrameInputQueue.push(&mAVFrames[i]); // 加入到输入帧队列

    // 线程池任务回调函数。通过多态读取调用从h264文件读取一个AVFrame到临时缓冲到mAVFrameInputQueue，指定位置是设置的缓冲区mAVFrames，然后取出到mAVFrameOutputQueue
    mTask.setTaskCallback(MediaSource::taskCallback, this);
}

MediaSource::~MediaSource(){
    Delete::release(mMutex);
}

// RtpSink调用getFrame和putFrame函数进行数据消费

AVFrame* MediaSource::getFrame() {
    MutexLockGuard mutexLockGuard(mMutex); // 使用互斥锁保护临界区

    // 输出队列为空，返回空指针
    if(mAVFrameOutputQueue.empty()) return nullptr;

    // 从输出队列中取出一个视频帧
    AVFrame* frame = mAVFrameOutputQueue.front();    
    mAVFrameOutputQueue.pop();

    return frame;
}

// 将一个AVFrame对象放入输入队列，然后将一个任务添加到线程池。
void MediaSource::putFrame(AVFrame* frame){
    MutexLockGuard mutexLockGuard(mMutex);

    mAVFrameInputQueue.push(frame);// 将视频帧放入待处理队列
    
    mEnv->threadPool()->addTask(mTask);// 添加任务到线程池
}

// 执行任务回调函数，其中任务回调函数会调用类的成员函数readFrame()
void MediaSource::taskCallback(void* arg){

    MediaSource* source = (MediaSource*)arg;
    // 多态调用H264FileMediaSource::readFrame，
    // 读取一个AVFrame到mAVFrameOutputQueue中
    source->readFrame(); 
}
