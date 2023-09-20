#ifndef _CONSTRUCT_H_
#define _CONSTRUCT_H_
#include <new>


/*
定义一系列用于构造和销毁对象的模板函数。destroy函数用于销毁对象，construct函数用于构造对象，可以处理不同数量的参数。这些函数通常与New.h一起使用，用于对象的生命周期管理。
*/

// 销毁对象
template <class T>
inline void destroy(T* p)
{
    p->~T();
}
// 构造对象
template <class T>
inline void construct(T* p)
{
    new (p) T();
}
// 构造对象，带有一个参数
template <class T, class T1>
inline void construct(T* p, const T1& a1)
{
    new (p) T(a1); // a1是任何的参数，构造线程池传进来参数线程的数量
}

template <class T, class T1, class T2>
inline void construct(T* p, const T1& a1, const T2& a2)
{
    new (p) T(a1, a2);
}

template <class T, class T1, class T2, class T3>
inline void construct(T* p, const T1& a1, const T2& a2, const T3& a3)
{
    new (p) T(a1, a2, a3);
}

template <class T, class T1, class T2, class T3, class T4>
inline void construct(T* p, const T1& a1, const T2& a2, const T3& a3, const T4& a4)
{
    new (p) T(a1, a2, a3, a4);
}

#endif //_CONSTRUCT_H_