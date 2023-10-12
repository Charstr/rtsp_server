

// Mutex.cpp
#include "Mutex.h"
#include "base/New.h"

Mutex* Mutex::createNew()
{
    //return new Mutex();
    // 创建并返回Mutex的实例
    return New<Mutex>::allocate();
}

Mutex::Mutex()
{
}

Mutex::~Mutex()
{
}

void Mutex::lock()
{
    mMutex.lock();// 上锁
}

void Mutex::unlock()
{
    mMutex.unlock();// 解锁
}

MutexLockGuard::MutexLockGuard(Mutex* mutex) :
    mLock(*mutex->get())
{
}
