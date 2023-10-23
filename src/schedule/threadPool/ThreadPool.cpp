#include "ThreadPool.h"
#include "base/Logging.h"
#include "base/New.h"
#include <algorithm>

ThreadPool* ThreadPool::createNew(int num) {
    return New<ThreadPool>::allocate(num);
}

/*---------------线程操作----------------*/
ThreadPool::ThreadPool(int num) :
    mThreads(num),// 设置线程池中线程的数量，vector存储
    mQuit(false)// 初始化终止标志为false
{
    mMutex = Mutex::createNew();
    mCondition = Condition::createNew();

    // 创建多个线程并设置处理任务的函数Thread::threadRun，对于每个线程都通过单个工作线程的函数ThreadPool::MThread::run调用threadPool->handleTask，从任务队列取出一个任务执行
    // 线程初始化后进入等待状态，等待任务队列中有任务可以执行。
    createThreads();
}


void ThreadPool::createThreads(){
    MutexLockGuard mutexLockGuard(mMutex);// 互斥锁保护线程池
    
    // 遍历线程池的大小，pthread_create创建线程MThread并设置线程执行的回调函数Thread::threadRun

    // 向任务队列添加任务的时候都通过信号量唤醒一个等待中的线程，当任务队列有任务时，通过单个工作线程的函数ThreadPool::MThread::run调用threadPool->handleTask，从任务队列取出一个任务执行
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

/*----------线程池处理任务---------------*/

// 向任务队列中添加一个任务，并唤醒一个等待等待中的线程去处理任务。
void ThreadPool::addTask(ThreadPool::Task& task){
    MutexLockGuard mutexLockGuard(mMutex);
    mTaskQueue.push(task);// 将任务加入队列
    mCondition->signal();// 唤醒一个等待的线程，接下来就要调用线程执行具体任务的函数threadRun再调run
}


// threadRun函数调用选出来某个线程处理任务
void ThreadPool::MThread::run(void* arg){
    ThreadPool* threadPool = (ThreadPool*)arg;
    threadPool->handleTask();    
}

// 处理任务的函数，不断从任务队列中获取任务并执行。
void ThreadPool::handleTask(){
    // 线程池没有终止时循环执行
    while(mQuit != true){
        Task task;

        {
            // 保证任务的添加和移除（获取）的互斥性
            MutexLockGuard mutexLockGuard(mMutex);

            // 任务队列为空时循环阻塞等待，这里会在没有任务也就是环形队列满了的时候阻塞
            if(mTaskQueue.empty())
                mCondition->wait(mMutex);// 等待条件变量通知，开启线程

            // 如果线程池终止标志为true，则退出循环
            if(mQuit == true) break;
            // 不退出但是此时队列为空就循环运行
            if(mTaskQueue.empty()) continue;
            
            // 用移动语义？
            task = mTaskQueue.front();// 取出队列中的任务

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
