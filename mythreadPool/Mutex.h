// Mutex.h
#ifndef _MUTEX_H_
#define _MUTEX_H_
#include <mutex>

class Mutex{
public:
    static Mutex* createNew();
    
    Mutex();
    ~Mutex();

    void lock(); // 上锁，阻塞当前线程直到获得锁
    void unlock();// 解锁，释放锁
    
    std::mutex* get() { return &mMutex; };

private:
    std::mutex mMutex;// 互斥锁对象

};

class MutexLockGuard
{
public:
    MutexLockGuard(Mutex* mutex);// 构造函数，自动上锁互斥锁
    ~MutexLockGuard();// 析构函数，自动解锁互斥锁

private:
    std::unique_lock<std::mutex> mLock;// 指向互斥锁的指针

};

#endif //_MUTEX_H_