#include <sys/time.h>
#include <time.h>

#include "Condition.h"
#include "base/New.h"

Condition* Condition::createNew()
{
    //return new Condition();
    // 创建并返回Condition的实例
    return New<Condition>::allocate();
}

Condition::Condition()
{
    pthread_cond_init(&mCond, NULL);// 初始化条件变量
}

Condition::~Condition()
{
    pthread_cond_destroy(&mCond);
}

void Condition::wait(Mutex* mutex)
{
    pthread_cond_wait(&mCond, mutex->get());// 等待条件变量，释放互斥锁并阻塞线程
}

bool Condition::waitTimeout(Mutex* mutex, int ms)
{
    struct timespec abstime;
    struct timespec now;

    clock_gettime(CLOCK_REALTIME, &now);

    abstime.tv_sec = now.tv_sec + ms/1000;
    abstime.tv_nsec = now.tv_nsec + ms%1000*1000*1000;
     // 带超时的等待条件变量,返回true表示等待成功,false表示等待超时
    if(pthread_cond_timedwait(&mCond, mutex->get(), &abstime) == 0)
        return true;
    else
        return false;
    
}

void Condition::signal()
{
    pthread_cond_signal(&mCond);
}

void Condition::broadcast()
{
    pthread_cond_broadcast(&mCond);
}
