#ifndef _ACCEPTOR_H_
#define _ACCEPTOR_H_
#include "schedule/Event.h"
#include "schedule/UsageEnvironment.h"
#include "server/InetAddress.h"
#include "server/TcpSocket.h"

/*

负责新连接 也只负责连接新连接 其他的多余的不管

Acceptor类是一个关键的组件，监听连接请求,处理新连接的接受和分发，它与事件调度器（UsageEnvironment）协同工作，确保在有新连接时能够触发回调函数进行处理。

*/

class Acceptor {
public:
	typedef void (*NewConnectionCallback)(void *data, int connfd);

	// 创建一个新的Acceptor实例
	static Acceptor *createNew(UsageEnvironment *env, const Ipv4Address &addr);

	Acceptor(UsageEnvironment *env, const Ipv4Address &addr);
	~Acceptor();
	// 检查是否正在监听
	bool listenning() const {
		return mListenning;
	}
	void listen(); // 开始监听连接

	// 设置处理新连接的回调函数，原来在cpp文件实现
	void setNewConnectionCallback(NewConnectionCallback cb, void *arg) {
		mNewConnectionCallback = cb;
		mArg = arg;
	}

private:
	static void readCallback(void *); // 静态函数，用作读取回调函数的指针。
	void handleRead(); // 处理读取操作。

private:
	UsageEnvironment *mEnv; // UsageEnvironment对象，用于事件调度
	Ipv4Address mAddr; // 监听地址
	std::shared_ptr<IOEvent> mAcceptIOEvent; // 指向IOEvent对象的指针。
	TcpSocket mSocket; // 监听套接字
	bool mListenning; // 是否正在监听连接
	NewConnectionCallback mNewConnectionCallback; // 用于处理新连接的回调函数。
	void *mArg; // 传递给回调函数的额外参数。
};

#endif //_ACCEPTOR_H_