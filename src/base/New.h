#ifndef _NEW_H_
#define _NEW_H_

#include <utility>

#include "Allocator.h"

// 销毁对象
template <class T>
inline void destroy(T *p) {
	p->~T();
}

template <class T, class... Args>
inline void construct(T *p, Args &&...args) {
	new (p) T(std::forward<Args>(args)...);
}

template <class T>
class New {
public:
	template <class... Args>
	static T *allocate(Args &&...args) {
		T *obj = static_cast<T *>(Allocator::allocate(sizeof(T)));
		construct(obj, std::forward<Args>(args)...); // 构造
		return obj;
	}
};

class Delete {
public:
	// 静态方法release，用于释放内存。它首先调用destroy函数销毁对象，然后使用Allocator类来释放内存。
	template <class T1>
	static void release(T1 *p) {
		destroy(p); // 析构
		Allocator::deallocate(p, sizeof(T1));
	}
};

#endif //_NEW_H_