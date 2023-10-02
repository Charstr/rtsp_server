#ifndef _NEW_H_
#define _NEW_H_

#include "Allocator.h"
#include "Construct.h"

/*
定义了New模板类和Delete类，用于在动态内存上构造和销毁对象。New类包括多个模板函数，允许分配内存并构造对象，可以处理不同数量的参数。Delete类包含一个模板函数，用于销毁对象并释放分配的内存。
*/

// New类中定义了一些静态方法，用于分配内存和执行构造函数。allocate方法提供了多个重载，可以根据传入的参数个数来选择适当的构造函数。基本思想是使用Allocator类来分配内存，然后使用Construct类来执行构造函数。
template <class T>
class New
{
public:
    // 分配内存并构造对象
    static T* allocate() {
        T* obj = (T*)Allocator::allocate(sizeof(T));// 强转指针类型为T类型
        construct(obj);
        return obj;
    }
    // 分配内存并构造对象，带有一个参数
    template <class T1>
    static T* allocate(const T1& a1) {
        T* obj = (T*)Allocator::allocate(sizeof(T));
        construct(obj, a1); // 
        return obj;
    }
    // New<EventScheduler>::allocate(type, evtFd);
    // 这个调用，将EventScheduler作为其类型参数，T 被实例化为 EventScheduler，T1 被实例化为 type 的类型，而 T2 被实例化为 evtFd 的类型。
    template <class T1, class T2>
    static T* allocate(const T1& a1, const T2& a2) {
        
        T* obj = (T*)Allocator::allocate(sizeof(T));
        /*
        New<EventScheduler>::allocate(type, evtFd);
        调用传过来的参数，obj的类型为T* 也就是EventScheduler*，a1和a2是通过引用传递的参数
        调用一个接受三个参数的函数模板construct, 在obj指向的内存位置上,使用a1和a2作为构造函数的参数,创建一个T类型的对象
        
        template <class T, class T1, class T2>
        inline void construct(T* p, const T1& a1, const T2& a2) {
            new (p) T(a1, a2);
        }
        construct(obj, a1, a2); 
        T推导为EventScheduler
        */
        construct(obj, a1, a2);
        return obj;
    }

    template <class T1, class T2, class T3>
    static T* allocate(const T1& a1, const T2& a2, const T3& a3) {
        T* obj = (T*)Allocator::allocate(sizeof(T));
        construct(obj, a1, a2, a3);
        return obj;
    }

    template <class T1, class T2, class T3, class T4>
    static T* allocate(const T1& a1, const T2& a2, const T3& a3, const T4& a4) {
        T* obj = (T*)Allocator::allocate(sizeof(T));
        construct(obj, a1, a2, a3, a4);
        return obj;
    }
};

class Delete
{
public:
    // 静态方法release，用于释放内存。它首先调用destroy函数销毁对象，然后使用Allocator类来释放内存。
    template <class T1>
    static void release(T1* p) {
        destroy(p);
        Allocator::deallocate(p, sizeof(T1));
    }

};

#endif //_NEW_H_