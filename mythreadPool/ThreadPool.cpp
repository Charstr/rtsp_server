
// ThreadPool.cpp
#include "ThreadPool.h"
#include <algorithm>

ThreadPool* ThreadPool::createNew(int num) {
    return new ThreadPool(num);
}

ThreadPool::ThreadPool(int num) :
    mThreads(num),
    mQuit(false)
{
    createThreads();
}

void ThreadPool::createThreads(){
    for(auto& th : mThreads)
        th = std::thread(&ThreadPool::handleTask, this);
}

void ThreadPool::cancelThreads(){
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mQuit = true; 
        mCondition.notify_all();
    }

    for(auto& th : mThreads)
        if(th.joinable())
            th.join();

    mThreads.clear();
}

void ThreadPool::addTask(ThreadPool::Task task){
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mTaskQueue.push(std::move(task));
        mCondition.notify_one();
    }
}

void ThreadPool::handleTask(){
    while(true){
        Task task;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait(lock, [this]{ return mQuit || !mTaskQueue.empty(); });
            if(mQuit && mTaskQueue.empty()) 
                return;
            task = std::move(mTaskQueue.front());
            mTaskQueue.pop();
        }
        task();
    }
}

ThreadPool::~ThreadPool(){
    cancelThreads();
}
