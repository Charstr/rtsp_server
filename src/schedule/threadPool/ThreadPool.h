#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <queue>
#include <vector>

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"


/*

线程池的核心思想是将多个任务分发给一组线程来执行，以提高并行性和任务处理效率。线程池中的线程会不断地从任务队列中取出任务并执行，直到线程池被终止。

通过使用互斥锁和条件变量，线程池能够实现线程之间的同步，确保任务队列的安全访问。

线程池可以复用线程，避免了频繁创建和销毁线程的开销，提高了程序的性能。

*/

// 对应于EventLoopThreadPool
class ThreadPool{
public:
    // Task类，用于表示要执行的任务。每个任务都包含一个回调函数和一个参数。

    class Task{
    public:
        typedef void (*TaskCallback)(void*);
        Task() { };
        
        // 设置任务回调函数及参数
        void setTaskCallback(TaskCallback cb, void* arg) {
            mTaskCallback = cb; mArg = arg;
        }

        // 执行任务的回调函数
        void handle() { 
            if(Task::mTaskCallback) 
                Task::mTaskCallback(mArg); 
        }

        // 拷贝赋值函数
        Task& operator=(const Task& task) {
            if(this != &task){
                this->mTaskCallback = task.mTaskCallback;
                this->mArg = task.mArg;
            }
            return *this;
        }
    private:
        void (*mTaskCallback)(void*); // 执行任务使用的回调函数
        void* mArg;
    };

    // 静态方法，用于创建线程池实例
    static ThreadPool* createNew(int num);
    // 构造函数，初始化线程池的成员变量，包括任务队列（mTaskQueue）、互斥锁（mMutex）、条件变量（mCondition）等。
    ThreadPool(int num);

    // 禁用部分函数
    ThreadPool(const ThreadPool & )=delete;
    ThreadPool(ThreadPool&&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;
    ThreadPool& operator=(ThreadPool&&)=delete;

    // 终止线程池，释放资源
    ~ThreadPool();

    // 添加任务到线程池，并唤醒一个等待的线程。
    void addTask(Task& task);

private:

    // 继承自Thread类，表示线程池中的工作线程
    class MThread : public Thread {
    protected:
        virtual void run(void *arg); // 重写线程执行函数
    };

    // 创建线程池中的线程，并启动
    void createThreads();// 创建线程
    // 终止线程池，设置终止标志为true，唤醒所有等待的线程，并等待它们终止。
    void cancelThreads();
    // 线程执行函数，从任务队列中取出任务并执行，直到线程池终止。
    void handleTask();

private:

    std::queue<Task> mTaskQueue;// 执行函数安全队列，即任务队列
    std::vector<MThread> mThreads;// 工作线程队列

    Mutex* mMutex;// 互斥锁，保证任务的添加和移除（获取）的互斥性

    Condition* mCondition;// 线程环境锁，可以让线程处于休眠或者唤醒状态，当任务队列为空时，线程应该等待（阻塞）

    bool mQuit; // 标志线程池是否终止
};

#endif //_THREADPOOL_H_