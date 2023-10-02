#ifndef _TCP_CONNECTION_H_
#define _TCP_CONNECTION_H_
#include "schedule/UsageEnvironment.h"
#include "schedule/Event.h"
#include "TcpSocket.h"
#include "base/Buffer.h"

/*
封装了与TCP连接相关的功能，包括创建、处理读写事件、处理错误以及处理断开连接等。

TcpConnection类使用回调函数来处理套接字的读、写和错误事件。这些回调函数会在事件发生时被调用，从而允许处理特定的事件。
使用Buffer类来管理输入和输出数据的缓冲区。可以将数据从套接字读入缓冲区，或者将数据从缓冲区写入套接字。
*/
class TcpConnection
{
public:
    typedef void (*DisconnectionCallback)(void*, int);
    // 构造函数，接受一个UsageEnvironment实例和一个套接字文件描述符
    TcpConnection(UsageEnvironment* env, int sockfd);
    virtual ~TcpConnection();
    // 设置断开连接回调函数
    void setDisconnectionCallback(DisconnectionCallback cb, void* arg);

protected:
      // 启用/禁用读,写,错误事件
    void enableReadHandling();
    void enableWriteHandling();
    void enableErrorHandling();
    void disableReadeHandling();
    void disableWriteHandling();
    void disableErrorHandling();

    // 处理读事件
    void handleRead();
    virtual void handleReadBytes();// 处理读字节
    virtual void handleWrite();
    virtual void handleError();

    void handleDisconnection();

private:
    // 静态回调函数，处理读,写,错误事件
    static void readCallback(void* arg);
    static void writeCallback(void* arg);
    static void errorCallback(void* arg);

protected:
    UsageEnvironment* mEnv;
    TcpSocket mSocket;
    IOEvent* mTcpConnIOEvent;
    // 回调函数和参数
    DisconnectionCallback mDisconnectionCallback;
    void* mArg;
    Buffer mInputBuffer;
    Buffer mOutBuffer;
    char mBuffer[2048];
};

#endif //_TCP_CONNECTION_H_