#include "Mutex.h"
#include "base/New.h"

Mutex *Mutex::createNew() {
	// return new Mutex();
	//  创建并返回Mutex的实例
	return New<Mutex>::allocate();
}

Mutex::Mutex() {
	pthread_mutex_init(&mMutex, NULL); // 初始化互斥锁
}

Mutex::~Mutex() {
	pthread_mutex_destroy(&mMutex); // 销毁互斥锁
}

void Mutex::lock() {
	pthread_mutex_lock(&mMutex); // 上锁
}

void Mutex::unlock() {
	pthread_mutex_unlock(&mMutex); // 解锁
}

MutexLockGuard::MutexLockGuard(Mutex *mutex) : mMutex(mutex) {
	mMutex->lock(); // 构造函数中自动上锁
}

MutexLockGuard::~MutexLockGuard() {
	mMutex->unlock(); // 析构函数中自动解锁
}
