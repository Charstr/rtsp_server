// ThreadPool.cpp
#include "ThreadPool.h"
#include "base/New.h"
#include <cstdint>
#include <memory>

std::shared_ptr<ThreadPool> ThreadPool::createNew(int numThreads) {
	return std::make_shared<ThreadPool>(numThreads);
}

// 设置线程的大小
ThreadPool::ThreadPool(uint16_t numThreads) : m_stop(false) {
	{
		if (numThreads < 1)
			m_threads = 1;
		else
			m_threads = numThreads;
	}
	// 启动指定数量的线程
	for (int i = 0; i < numThreads; ++i) {
		m_threadPool.emplace_back([this] {
			while (!this->m_stop.load()) { // 不停止的话，一直在运行
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(this->m_mutex);
					// 不为空且没有停止那就可以继续运行
					this->m_cond.wait(lock, [this] {
						return this->m_stop.load() || !this->m_tasks.empty();
					});
					if (this->m_stop && this->m_tasks.empty())
						return;
					// 取出来一个任务执行
					task = std::move(this->m_tasks.front());
					this->m_tasks.pop();
				}
				task(); // 执行的任务也就是handletask
			}
		});
	}
}

void ThreadPool::stop() {
	m_stop.store(true); // 原子变量不需要加锁

	m_cond.notify_all();
	for (auto &thread : m_threadPool) {
		if (thread.joinable())
			thread.join();
	}
}

ThreadPool::~ThreadPool() {
	stop();
}
