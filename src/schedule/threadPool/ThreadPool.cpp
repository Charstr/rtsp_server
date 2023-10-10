#include "ThreadPool.h"
#include "base/Logging.h"
#include "base/New.h"
#include <algorithm>

ThreadPool* ThreadPool::createNew(int num) {
    return New<ThreadPool>::allocate(num);
}


// 初始化线程池的成员变量，并创建线程。
ThreadPool::ThreadPool(int num) :
    mThreads(num),// 设置线程池中线程的数量，vector存储
    mQuit(false)// 初始化终止标志为false
{
    mMutex = Mutex::createNew();// 创建互斥锁
    mCondition = Condition::createNew();// 创建条件变量

    createThreads();// 创建线程,启动线程，每个线程执行handleTask方法
}

// 创建线程的函数，遍历线程组，启动每一个线程，分配工作线程
void ThreadPool::createThreads(){
    MutexLockGuard mutexLockGuard(mMutex);// 互斥锁保护线程池
    // 遍历线程池 pthread_create创建线程并设置mIsStart=true
    // 设置某个线程执行的回调函数Thread::threadRun，通过单个工作线程的函数ThreadPool::MThread::run
    // 运行
    for(std::vector<MThread>::iterator it = mThreads.begin(); it != mThreads.end(); ++it)
        (*it).start(this);
}

// 取消线程的函数，设置标志位使线程退出，并等待每一个线程结束。
void ThreadPool::cancelThreads(){
    MutexLockGuard mutexLockGuard(mMutex);// 使用互斥锁保护线程池

    mQuit = true; // 设置线程池终止标志为true
    mCondition->broadcast();// 通知，唤醒所有工作线程

    // 将线程加入到等待队列
    for(std::vector<MThread>::iterator it = mThreads.begin(); it != mThreads.end(); ++it)
        (*it).join();

    mThreads.clear();// 清空线程池中的线程
}

// 线程执行任务处理函数
void ThreadPool::MThread::run(void* arg){
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->handleTask();    
}

// 向任务队列中添加一个任务，并唤醒一个等待等待中的线程去处理任务。
void ThreadPool::addTask(ThreadPool::Task& task){
    MutexLockGuard mutexLockGuard(mMutex);
    mTaskQueue.push(task);// 将任务加入队列
    mCondition->signal();// 唤醒一个等待的线程
}

// 处理任务的函数，不断从任务队列中获取任务并执行。
void ThreadPool::handleTask(){
    // 线程池没有终止时循环执行
    while(mQuit != true){
        Task task;

        {
            // 保证任务的添加和移除（获取）的互斥性
            MutexLockGuard mutexLockGuard(mMutex);
            // 任务队列为空时循环阻塞等待
            if(mTaskQueue.empty())
                mCondition->wait(mMutex);// 等待条件变量通知，开启线程
            // 如果线程池终止标志为true，则退出循环
            if(mQuit == true) break;

            if(mTaskQueue.empty()) continue;
            
            // 用移动语义？
            task = std::move(mTaskQueue.front());// 取出队列中的任务

            mTaskQueue.pop();// 移除队列头部的任务
        }

        task.handle();// 执行任务的回调函数
    }
}


ThreadPool::~ThreadPool(){
    cancelThreads();// 终止线程

    Delete::release(mMutex);// 释放互斥锁
    Delete::release(mCondition);// 释放条件变量
}
