#include <assert.h>
#include <functional>

#include "TcpServer.h"
#include "base/Logging.h"
#include "base/New.h"

TcpServer::TcpServer(UsageEnvironment *env, const Ipv4Address &addr) : mEnv(env), mAddr(addr) {
	// 创建Acceptor，用于监听新连接
	mAcceptor = Acceptor::createNew(env, addr);
	assert(mAcceptor);

	/*
	1.
	Acceptor用于接受新的连接。会创建一个新的tcp连接mSocket，到这里Acceptor创建了新的socket套接字、绑定的端口和IP，但是没有进行监听listen和接受连接accept。创建了接受连接的mAcceptIOEvent事件

	2.
	在server->start的时候Acceptor::listen开启对mSocket的listen，同时把mAcceptIOEvent注册到EventScheduler。accept在mAcceptIOEvent事件调度新连接进来时候进行。

	3.
	当mAcceptIOEvent事件触发也就是有新的连接过来的时候，调用回调函数Acceptor::readCallback，使用处理函数acceptor->handleRead接受这个连接然后返回通信的connfd。调用回调函数TcpServer::newConnectionCallback处理这个连接（connfd），通过多态RtspServer::handleNewConnection进行处理，进入到了RtspServer相关处理过程中
	*/

	// 绑定的是静态函数，所以不需要this指针
	mAcceptor->setNewConnectionCallback(
		std::bind(&TcpServer::newConnectionCallback, std::placeholders::_1, std::placeholders::_2),
		this);
}

void TcpServer::newConnectionCallback(void *arg, int connfd) {
	// 处理新连接的回调函数
	TcpServer *tcpServer = (TcpServer *)arg;
	// 多态，实际调用的是RtspServer::handleNewConnection
	tcpServer->handleNewConnection(connfd);
}

// server->start();实际调用这个父类的函数，开启对mSocket的监听，同时把mAcceptIOEvent注册到EventScheduler
void TcpServer::start() {
	mAcceptor->listen();
}

TcpServer::~TcpServer() {
	// 要不要关闭？
	Delete::release(mAcceptor);
}
