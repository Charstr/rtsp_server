#ifndef _MUTEX_H_
#define _MUTEX_H_
#include <pthread.h>

/*

Mutex类封装了互斥锁的操作，MutexLockGuard类是一个辅助类，用于在构造函数中自动上锁，在析构函数中自动解锁，确保在作用域结束时正确管理互斥锁

*/

class Mutex {
public:
	static Mutex *createNew();

	Mutex();
	~Mutex();

	void lock();   // 上锁，阻塞当前线程直到获得锁
	void unlock(); // 解锁，释放锁

	// / 获取底层的pthread_mutex_t
	pthread_mutex_t *get() { return &mMutex; };

private:
	pthread_mutex_t mMutex; // 互斥锁对象
};

class MutexLockGuard {
public:
	MutexLockGuard(Mutex *mutex); // 构造函数，自动上锁互斥锁
	~MutexLockGuard();			  // 析构函数，自动解锁互斥锁

private:
	Mutex *mMutex; // 指向互斥锁的指针
};

#endif //_MUTEX_H_