#ifndef _ACCEPTOR_H_
#define _ACCEPTOR_H_
#include "UsageEnvironment.h" 
#include "Event.h"
#include "server/InetAddress.h"
#include "server/TcpSocket.h"

/*
Acceptor类是一个关键的组件，监听连接请求,处理新连接的接受和分发，它与事件调度器（UsageEnvironment）协同工作，确保在有新连接时能够触发回调函数进行处理。

*/
class Acceptor
{
public:
    typedef void(*NewConnectionCallback)(void* data, int connfd);
    // 创建一个新的Acceptor实例
    static Acceptor* createNew(UsageEnvironment* env, const Ipv4Address& addr);

    Acceptor(UsageEnvironment* env, const Ipv4Address& addr);
    ~Acceptor();
    // 检查是否正在监听
    bool listenning() const { return mListenning; }
    void listen();// 开始监听连接
    // 设置新连接回调函数
    void setNewConnectionCallback(NewConnectionCallback cb, void* arg);

private:
    static void readCallback(void*);
    void handleRead();

private:
    UsageEnvironment* mEnv; // UsageEnvironment对象，用于事件调度
    Ipv4Address mAddr;// 监听地址
    IOEvent* mAcceptIOEvent;// 接受连接的IO事件
    TcpSocket mSocket;// 监听套接字
    bool mListenning;// 是否正在监听连接
    NewConnectionCallback mNewConnectionCallback;// 新连接回调函数
    void* mArg; // 回调函数参数
};

#endif //_ACCEPTOR_H_