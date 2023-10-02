#include "MediaSource.h"
#include "base/Logging.h"
#include "base/New.h"

MediaSource::MediaSource(UsageEnvironment* env) :
    mEnv(env)
{
    mMutex = Mutex::createNew();
    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)
        mAVFrameInputQueue.push(&mAVFrames[i]);// 初始化视频帧缓冲池
    // 设置回调MediaSource::taskCallback
    // 调用source->readFrame()产生数据
    mTask.setTaskCallback(MediaSource::taskCallback, this);
}

MediaSource::~MediaSource()
{
    //delete mMutex;
    Delete::release(mMutex);
}

AVFrame* MediaSource::getFrame()
{
    MutexLockGuard mutexLockGuard(mMutex); // 使用互斥锁保护临界区

    if(mAVFrameOutputQueue.empty())
    {
        return NULL;// 输出队列为空，返回空指针
    }

    // 从输出队列中取出一个视频帧
    AVFrame* frame = mAVFrameOutputQueue.front();    
    mAVFrameOutputQueue.pop();

    return frame;
}

// 将一个AVFrame对象放入输入队列，然后将一个任务添加到线程池。
void MediaSource::putFrame(AVFrame* frame)
{
    MutexLockGuard mutexLockGuard(mMutex);

    mAVFrameInputQueue.push(frame);// 将视频帧放入待处理队列
    
    mEnv->threadPool()->addTask(mTask);// 添加任务到线程池
}

// 执行任务回调函数，其中任务回调函数会调用类的成员函数readFrame()
void MediaSource::taskCallback(void* arg)
{
    MediaSource* source = (MediaSource*)arg;
    source->readFrame(); // 调用派生类中的读取视频帧函数
}
