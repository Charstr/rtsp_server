// Thread.h
#ifndef _THREAD_H_
#define _THREAD_H_
#include <thread>

class Thread
{
public:
    virtual ~Thread();
    
    bool start(void *arg);
    bool detach(); 
    bool join(); 
    std::thread::id getThreadId() const;

protected:
    Thread();
    virtual void run(void *arg) = 0;

private:
    static void threadRun(Thread *t, void *arg);

private:
    void *mArg;
    bool mIsStart;
    bool mIsDetach;
    std::thread mThread; 
};

#endif //_THREAD_H_

