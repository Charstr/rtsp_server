// ThreadPool.h
#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <queue>
#include <vector>
#include <thread>
#include <functional>

class ThreadPool{
public:
    using Task = std::function<void(void)>;

    static ThreadPool* createNew(int num);
    ThreadPool(int num);

    ThreadPool(const ThreadPool & )=delete;
    ThreadPool(ThreadPool&&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;
    ThreadPool& operator=(ThreadPool&&)=delete;

    ~ThreadPool();

    void addTask(Task task);

private:
    void createThreads();
    void cancelThreads();
    void handleTask();

private:

    std::queue<Task> mTaskQueue;
    std::vector<std::thread> mThreads;

    std::mutex mMutex;
    std::condition_variable mCondition;

    bool mQuit; 
};

#endif //_THREADPOOL_H_
