
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
}

Condition::~Condition()
{
}

void Condition::wait(std::unique_lock<std::mutex>& lock)
{
    mCond.wait(lock);
}

bool Condition::waitTimeout(std::unique_lock<std::mutex>& lock, int ms)
{
    return mCond.wait_for(lock, std::chrono::milliseconds(ms)) == std::cv_status::no_timeout;
}

void Condition::signal()
{
    mCond.notify_one();
}

void Condition::broadcast()
{
    mCond.notify_all();
}
