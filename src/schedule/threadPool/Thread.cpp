#include "Thread.h"

Thread::Thread() :
    mArg(NULL),
    mIsStart(false),
    mIsDetach(false)
{

}

Thread::~Thread(){
    // 开始运行且不是工作线程
    if(mIsStart == true && mIsDetach == false)
        detach(); // 线程分离
}
    
bool Thread::start(void *arg){
    mArg = arg;
 
    // 创建这个线程的时候，设置了唤醒这个线程后线程执行具体任务的回调函数
    // 每个线程都有对应的ID
    if(pthread_create(&mThreadId, NULL, threadRun, this))
        return false;

    mIsStart = true;
    return true;
}

bool Thread::detach(){
    if(mIsStart != true)
        return false;

    if(mIsDetach == true)
        return true;
    // 线程分离
    if(pthread_detach(mThreadId))
        return false;

    mIsDetach = true;

    return true;
}

// 把该线程join到当前线程上
bool Thread::join(){
    if(mIsStart != true || mIsDetach == true)
        return false;

    if(pthread_join(mThreadId, NULL))
        return false;
    
    return true;
}

bool Thread::cancel(){
    if(mIsStart != true) return false;
    if(pthread_cancel(mThreadId)) return false;
    mIsStart = false;

    return true;
}

pthread_t Thread::getThreadId() const{
    return mThreadId;
}

// 处理线程池任务的回调函数
void *Thread::threadRun(void *arg){
    Thread* thread = (Thread*)arg;
    // 调用的是ThreadPool::MThread::run
    thread->run(thread->mArg);
    return nullptr;
}
