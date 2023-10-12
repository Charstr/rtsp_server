#include "MediaSource.h"
#include "base/Logging.h"
#include "base/New.h"
#include <cstddef>

MediaSource::MediaSource(UsageEnvironment* env) :
    mEnv(env)
{
    mMutex = Mutex::createNew();

    /*

    视频帧缓冲区mAVFrames大小为DEFAULT_FRAME_NUM，也就是环形队列的大小。初始化的时候把mAVFrames每个元素的指针插入到mAVFrameInputQueue，缓冲区所在的位置是固定的，在mAVFrameInputQueue和mAVFrameOutputQueue之间传递的是缓冲区的指针，所以就是缓冲区的数据承载着读取的一个AVFrame，其指针存在两个队列中互相流转

    环形队列怎么生效的：
    1. 线程池中的每个线程执行的任务mTask通过回调都会调用readFrame从文件序号读取视频帧生产数据,存在mAVFrameInputQueue，然后再push给mAVFrameOutputQueue。
    2. 当定时事件mTimerEvent触发的时候，会回调设置的mTimerEvent->setTimeoutCallback(RtpSink::timeoutCallback);RtpSink::timeoutCallback开始消费数据。getFrame从mAVFrameOutputQueue读取视频帧，然后rtpSink->handleFrame通过rtp包发送出去，再putFrame将读取的帧插入mAVFrameInputQueue。
    这样就构成了一个环形队列
    */

    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)
        mAVFrameInputQueue.push(&mAVFrames[i]);

    // 设置线程池任务的回调函数MediaSource::taskCallback。

    // 线程池任务回调函数。线程池中的每个线程运行的时候，调用ThreadPool::MThread::run
    // 从任务队列mTaskQueue中取出一个mTask运行，设置mTask的回调函数MediaSource::taskCallback
    // 通过多态读取调用从h264文件读取一个AVFrame

    // 确保当有新的视频帧需要处理时，可以立即分配线程来处理这个任务，而不是等待线程池中的线程执行完当前任务后再去获取新的任务。
    // mTask的任务是取读取一个AVFrame，所以设置这一个回调
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

    /*
    添加到线程池的任务队列，创建线程的时候设置有处理任务的函数 Thread::threadRun，通过多态调用ThreadPool::MThread::run处理任务，调用线程池的ThreadPool::handleTask函数，从任务队列mTaskQueue取出来任务并调用对应任务的回调函数（这里设置的是MediaSource::taskCallback，通过多态调用H264FileMediaSource::readFrame从h264文件读取一个AVFrame到mAVFrameOutputQueue）执行任务
    */

    // 每当一个新的帧被放入输入队列时，就需要一个新的任务来处理它，都会添加一个新的任务到任务队列，
    // 立即通知线程池,避免消费者线程空等。
    // 任务的回调在MediaSource构造函数设置，读取一个AVFrame到队列
    mEnv->threadPool()->addTask(mTask);
}

// 执行任务回调函数，其中任务回调函数会调用类的成员函数readFrame()
void MediaSource::taskCallback(void* arg){

    MediaSource* source = (MediaSource*)arg;
    // 多态调用H264FileMediaSource::readFrame，
    // 读取一个AVFrame到mAVFrameOutputQueue中
    source->readFrame(); 
}
