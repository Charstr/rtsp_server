// Condition.h
#ifndef _CONDITION_H_
#define _CONDITION_H_
#include <condition_variable>
#include <mutex>

#include "Mutex.h"

class Condition
{
public:
    static Condition* createNew();  // 创建并返回Condition的实例
    
    Condition();// 构造函数，初始化条件变量
    ~Condition();// 析构函数，销毁条件变量

    void wait(std::unique_lock<std::mutex>& lock); // 等待条件变量，释放互斥锁并阻塞线程
    bool waitTimeout(std::unique_lock<std::mutex>& lock, int ms);// 带超时的等待条件变量，ms为毫秒
    void signal();// 唤醒一个等待的线程
    void broadcast();// 唤醒所有等待的线程

private:

    std::condition_variable mCond;// 条件变量对象
};

#endif //_CONDITION_H_

