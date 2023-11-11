// ThreadPool.h
#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
	static ThreadPool *createNew(int numThreads);

	ThreadPool(uint16_t numThreads);
	// 模板的实现一般都需要放在头文件中，以便在实例化模板的地方能够看到完整的模板定义。
	// 如果将模板的实现放在.cpp文件中，会导致链接时找不到模板的定义，从而引发链接错误。
	template <class F, class... Args>
	auto addTask(F &&f, Args &&...args) -> std::future<decltype(f(args...))>;
	void stop();

	~ThreadPool();

private:
	std::atomic_bool m_stop;
	std::mutex m_mutex;
	std::condition_variable m_cond;
	std::atomic_uint16_t m_threads;
	std::vector<std::thread> m_threadPool;
	std::queue<std::function<void()>> m_tasks;
};

template <class F, class... Args>
auto ThreadPool::addTask(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
	using retType = decltype(f(args...)); // 返回值类型
	if (m_stop.load())
		return std::future<retType>{};

	auto task = std::make_shared<std::packaged_task<retType()>>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...));
	// 获取任务的返回值
	std::future<retType> ret = task->get_future();
	{
		std::lock_guard<std::mutex> lguard(m_mutex);
		// 提交任务到队列
		m_tasks.emplace([task] {
			(*task)();
		});
	}
	m_cond.notify_one(); // 通知一个线程
	return ret;
}

#endif