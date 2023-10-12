// Thread.cpp
#include "Thread.h"

Thread::Thread() :
    mArg(NULL),
    mIsStart(false),
    mIsDetach(false)
{

}

Thread::~Thread(){
    if(mIsStart == true && mIsDetach == false)
        detach(); 
}
    
bool Thread::start(void *arg){
    mArg = arg;
    mThread = std::thread(threadRun, this, arg);
    mIsStart = true;
    return true;
}

bool Thread::detach(){
    if(mIsStart != true)
        return false;

    if(mIsDetach == true)
        return true;

    mThread.detach();
    mIsDetach = true;

    return true;
}

bool Thread::join(){
    if(mIsStart != true || mIsDetach == true)
        return false;

    if(mThread.joinable()){
        mThread.join();
        return true;
    }
    
    return false;
}

std::thread::id Thread::getThreadId() const{
    return mThread.get_id();
}

void Thread::threadRun(Thread *t, void *arg){
    t->run(arg);
}
