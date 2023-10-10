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
    // 创建这个线程的时候设置的回调函数
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

void *Thread::threadRun(void *arg){
    Thread* thread = (Thread*)arg;
    thread->run(thread->mArg);
    return nullptr;
}
