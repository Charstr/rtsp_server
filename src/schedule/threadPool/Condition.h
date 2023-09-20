#ifndef _CONDITION_H_
#define _CONDITION_H_
#include <pthread.h>

#include "Mutex.h"

/*
Mutex和Condition类是用于线程同步的关键组件，它们配合使用，确保线程池的任务队列在多线程环境下安全访问和操作。Mutex类封装了互斥锁的创建、销毁、上锁和解锁操作，而Condition类封装了条件变量的创建、销毁、等待、超时等操作。用于实现线程之间的协作。

*/
class Condition
{
public:
    static Condition* createNew();  // 创建并返回Condition的实例
    
    Condition();// 构造函数，初始化条件变量
    ~Condition();// 析构函数，销毁条件变量

    void wait(Mutex* mutex); // 等待条件变量，释放互斥锁并阻塞线程
    bool waitTimeout(Mutex* mutex, int ms);// 带超时的等待条件变量，ms为毫秒
    void signal();// 唤醒一个等待的线程
    void broadcast();// 唤醒所有等待的线程

private:
    pthread_cond_t mCond;// 条件变量对象
};

#endif //_CONDITION_H_