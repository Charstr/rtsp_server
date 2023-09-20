#include "ThreadPool.h"
#include "base/Logging.h"
#include "base/New.h"

ThreadPool* ThreadPool::createNew(int num)
{
    //return new ThreadPool(num);
    // 创建并返回一个ThreadPool实例
    // 这时候申请的内存大小就是ThreadPool大小
    return New<ThreadPool>::allocate(num);
}

ThreadPool::ThreadPool(int num) :
    mThreads(num),// 设置线程池中线程的数量
    mQuit(false)// 初始化终止标志为false
{
    mMutex = Mutex::createNew();// 创建互斥锁
    mCondition = Condition::createNew();// 创建条件变量

    createThreads();// 创建线程,启动线程，每个线程执行handleTask方法
}

ThreadPool::~ThreadPool()
{
    cancelThreads();// 终止线程
    //delete mMutex;
    //delete mCondition;
    Delete::release(mMutex);// 释放互斥锁
    Delete::release(mCondition);// 释放条件变量
}

void ThreadPool::addTask(ThreadPool::Task& task)
{
    MutexLockGuard mutexLockGuard(mMutex);// 使用互斥锁保护任务队列
    mTaskQueue.push(task);// 将任务加入队列
    mCondition->signal();// 唤醒一个等待的线程
}

void ThreadPool::handleTask()
{
    while(mQuit != true)// 线程池没有终止时循环执行
    {
        Task task;
        {
            MutexLockGuard mutexLockGuard(mMutex);// 使用互斥锁保护任务队列
            // 任务队列为空时循环阻塞等待
            if(mTaskQueue.empty())
                mCondition->wait(mMutex);
        
            if(mQuit == true)// 如果线程池终止标志为true，则退出循环
                break;

            if(mTaskQueue.empty())
                continue;

            task = mTaskQueue.front();// 取出队列中的任务

            mTaskQueue.pop();// 移除队列头部的任务
        }

        task.handle();// 执行任务的回调函数
    }
}

void ThreadPool::createThreads()
{
    MutexLockGuard mutexLockGuard(mMutex);// 使用互斥锁保护线程池
    // 遍历线程的列表创建线程
    for(std::vector<MThread>::iterator it = mThreads.begin(); it != mThreads.end(); ++it)
        (*it).start(this);// 启动线程，每个线程执行handleTask方法
}

void ThreadPool::cancelThreads()
{
    MutexLockGuard mutexLockGuard(mMutex);// 使用互斥锁保护线程池

    mQuit = true; // 设置线程池终止标志为true
    mCondition->broadcast();// 唤醒所有等待的线程
    for(std::vector<MThread>::iterator it = mThreads.begin(); it != mThreads.end(); ++it)
        (*it).join();// 等待线程终止

    mThreads.clear();// 清空线程池中的线程
}

void ThreadPool::MThread::run(void* arg)
{
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->handleTask();    // 线程执行任务处理函数
}