#ifndef _THREAD_H_
#define _THREAD_H_
#include <pthread.h>


// Thread类，用于创建和管理线程。

class Thread
{
public:
    virtual ~Thread();
    
    // 线程的基本操作
    bool start(void *arg);
    bool detach(); // 将线程设置为分离状态，使线程在结束时自动释放资源。
    bool join(); // 等待线程结束，并返回线程的返回值。
    bool cancel();
    pthread_t getThreadId() const;

protected:
    Thread();
    // 纯虚函数，需要子类实现，用于执行线程的具体逻辑。
    virtual void run(void *arg) = 0;

private:

    // 线程执行的函数
    static void *threadRun(void *);

private:
    void *mArg;
    bool mIsStart;
    bool mIsDetach;
    pthread_t mThreadId; // 每一个线程一个ID
};

#endif //_THREAD_H_

