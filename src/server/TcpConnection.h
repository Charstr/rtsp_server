#ifndef _TCP_CONNECTION_H_
#define _TCP_CONNECTION_H_
#include "TcpSocket.h"
#include "base/Buffer.h"
#include "schedule/Event.h"
#include "schedule/UsageEnvironment.h"

/*

1.
封装了已建立的TCP连接,以及控制该TCP连接的一些方法,还有连接发生后的各种事件的处理函数,以及这个连接的服务端客户端信息。使用回调函数会在事件发生时来处理套接字的读、写和错误事件。

2.
Acceptor和TcpConnection应该是兄弟关系,Acceptor对服务器监听套接字mSocket.fd()及其相关方法进行封装(监听,接受连接,分发连接等),而TcpConnection对连接套接字connfd及其相关方法进行封装(读消息事件,发送消息事件,连接关闭事件,错误时间等)

使用Buffer类来管理输入和输出数据的缓冲区。可以将数据从套接字读入缓冲区，或者将数据从缓冲区写入套接字。


*/
class TcpConnection {
public:
	using DisconnectionCallback = std::function<void(void *, int)>;

	// typedef void (*DisconnectionCallback)(void *, int);
	// 构造函数，接受一个UsageEnvironment实例和一个套接字文件描述符
	TcpConnection(UsageEnvironment *env, int sockfd);
	virtual ~TcpConnection();

	// 设置断开连接回调函数，默认在cpp文件实现
	void setDisconnectionCallback(DisconnectionCallback cb, void *arg) {
		mDisconnectionCallback = cb;
		mArg = arg;
	}

protected:
	// 启用/禁用读,写,错误事件
	void enableReadHandling();
	void enableWriteHandling();
	void enableErrorHandling();
	void disableReadeHandling();
	void disableWriteHandling();
	void disableErrorHandling();

	// 负责处理TCP连接的可读事件,把客户端发来的数据拷贝到用户缓冲区
	// 也就是mInputBuffer,接着调用connectionCallback_[连接建立后的处理函数]
	void handleRead();
	virtual void handleReadBytes(); // 解析读取到的字节
	virtual void handleWrite();
	virtual void handleError();

	void handleDisconnection();

private:
	// 静态回调函数，处理读,写,错误事件
	static void readCallback(void *arg);
	static void writeCallback(void *arg);
	static void errorCallback(void *arg);

protected:
	UsageEnvironment *mEnv;
	TcpSocket mSocket; // 保存已连接套接字文件描述符

	IOEvent *mTcpConnIOEvent; // 上边fd对应的IO事件，在构造函数中创建并注册到事件调度中

	// 回调函数和参数
	DisconnectionCallback mDisconnectionCallback;

	void *mArg;
	// 缓冲区，输入输出buffer
	Buffer mInputBuffer; // 从用户侧接收到的数据缓存
	Buffer mOutBuffer; // 输出数据的缓存

	char mBuffer[2048]; // 临时的缓冲区，作用是暂存一下然后拷贝到mOutBuffer中
};

#endif //_TCP_CONNECTION_H_