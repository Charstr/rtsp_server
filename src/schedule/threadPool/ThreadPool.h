#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <queue>
#include <vector>

#include "Thread.h"
#include "Mutex.h"
#include "Condition.h"

/*

ThreadPool类是一个简单的线程池实现，允许用户向线程池中添加任务，然后由线程池中的线程来执行这些任务。以下是该类的一些关键点：



线程池的核心思想是将多个任务分发给一组线程来执行，以提高并行性和任务处理效率。线程池中的线程会不断地从任务队列中取出任务并执行，直到线程池被终止。通过使用互斥锁和条件变量，线程池能够实现线程之间的同步，确保任务队列的安全访问。这种设计使得线程池可以有效地复用线程，避免了频繁创建和销毁线程的开销，提高了程序的性能。
*/
// 对应于EventLoopThreadPool
class ThreadPool
{
public:
    // Task类，用于表示要执行的任务。每个任务都包含一个回调函数和一个参数。
    class Task
    {
    public:
        typedef void (*TaskCallback)(void*);
        Task() { };
        
        // 设置任务回调函数的参数
        void setTaskCallback(TaskCallback cb, void* arg) {
            mTaskCallback = cb; mArg = arg;
        }

        // 执行任务回调函数
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
        void (*mTaskCallback)(void*);
        void* mArg;
    };




    static ThreadPool* createNew(int num);
    // 初始化了线程池的互斥锁、条件变量、线程数量和终止标志。
    ThreadPool(int num);
    ~ThreadPool();
    // 向线程池中添加任务，它会将任务加入任务队列并唤醒等待的线程。
    void addTask(Task& task);

private:

    // 线程池中线程的实现，它继承自Thread类，其run方法执行handleTask方法
    class MThread : public Thread
    {
    protected:
        virtual void run(void *arg);
    };
    // 创建线程池中的线程，并启动它们，每个线程都执行handleTask方法。
    void createThreads();// 创建线程
    // 终止线程池，设置终止标志为true，唤醒所有等待的线程，并等待它们终止。
    void cancelThreads();
    // 线程池中线程的执行函数，会从任务队列中取出任务并执行，直到线程池终止。
    void handleTask();

private:
    std::queue<Task> mTaskQueue;// 任务队列
    Mutex* mMutex;// 互斥锁，保护任务队列
    Condition* mCondition;// 条件变量，用于线程同步
    std::vector<MThread> mThreads;// 线程池中的线程
    bool mQuit; // 标志线程池是否终止
};

#endif //_THREADPOOL_H_