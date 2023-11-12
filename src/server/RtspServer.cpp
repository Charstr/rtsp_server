#include <algorithm>
#include <assert.h>
#include <memory>
#include <mutex>
#include <stdio.h>

#include "RtspServer.h"
#include "base/New.h"

std::shared_ptr<RtspServer> RtspServer::createNew(UsageEnvironment *env, Ipv4Address &addr) {
	return std::make_shared<RtspServer>(env, addr);
}

RtspServer::RtspServer(UsageEnvironment *env, const Ipv4Address &addr) : TcpServer(env, addr) {

	/*
	1.
	Acceptor用于接受新的连接。会创建一个新的tcp连接mSocket，到这里Acceptor创建了新的socket套接字、绑定的端口和IP，但是没有进行监听listen和接受连接accept。创建了接受连接的mAcceptIOEvent事件

	2.
	在server->start的时候Acceptor::listen开启对mSocket的listen，同时把mAcceptIOEvent注册到EventScheduler。当轮询到mAcceptIOEvent触发新连接进来时候，accept在mAcceptIOEvent事件调度触发。

	3.
	当mAcceptIOEvent事件触发也就是有新的连接过来的时候，调用回调函数Acceptor::readCallback，使用处理函数acceptor->handleRead接受这个连接然后返回通信的connfd。调用回调函数TcpServer::newConnectionCallback处理这个连接（connfd），通过多态RtspServer::handleNewConnection进行处理，进入到了RtspServer相关处理过程中

	4.
	TcpServer构造函数设置了Acceptor::handleRead接收连接的回调函数，通过多态调用RtspServer::handleNewConnection。

	5.
	RtspServer::handleNewConnection根据已经建立连接的connfd,创建RtspConnection对象（其中创建一个TcpConnection，构造函数根据Acceptor类accpet接受连接后返回的connfd创建了用于传输的事件mTcpConnIOEvent，设置读写异常回调函数，默认启用只读事件TcpConnection::readCallback，并添加mTcpConnIOEvent到EventScheduler，获取客户端IPmPeerIp，并设置了是否mIsRtpOverTcp。mTcpConnIOEvent事件发生的时候实际就是在传输数据，而触发事件mTriggerEvent是断开连接的。）。

	6.
	接着设置单个RtspConnection关闭连接的回调函数RtspServer::disconnectionCallback（在TcpConnection::handleRead处理读事件客户端关闭连接或读取失败及RtspConnection::handleReadBytes进行option解析错误的时候调用该回调函数，把要断开连接的connfd加入到要断开连接的队列mDisconnectionlist中。并添加断开连接的触发事件mTriggerEvent到调度器）

	7. 最后把新连接的connfd和对应的RtspConnection* conn加入到已建立连接的map中mConnections
	*/

	// 8.
	// 这里创建触发事件，设置触发事件的回调函数triggerCallback，回调函数遍历所有要关闭的连接描述符，从mConnections找到对应的RtspConnection释放内存关闭资源，然后从mConnections中删除对应的connfd的值。最后把map清空。

	// 上边完成了接受连接并进行处理的函数
	// 这里设置触发事件，处理mDisconnectionlist中所有要断开的连接
	mTriggerEvent = TriggerEvent::createNew(this);
	mTriggerEvent->setTriggerCallback(RtspServer::triggerCallback);
}

/*
1.
当有新的连接进来，epoll注意到有mAcceptIOEvent事件发生，调用回调函数Acceptor::readCallback用socket的accept用函数接受连接返回一个server和客户端通信的connfd，然后调用处理这个connfd处理连接的回调函数Acceptor::mNewConnectionCallback进行处理（在TcpServer的构造函数中设置的mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback,
this);实际是通过多态调用的 RtspServer::handleNewConnection运行）

*/

// mAcceptIOEvent事件发生，设置对已建立连接的处理
void RtspServer::handleNewConnection(int connfd) {

	// 1.
	// 根据已经建立连接的connfd,创建RtspConnection对象。其中创建一个TcpConnection，构造函数中创建了IOEvent事件mTcpConnIOEvent，设置读写异常回调函数，默认启用只读事件，并添加传输数据的mTcpConnIOEvent到EventScheduler，获取客户端IPmPeerIp，并设置了是否mIsRtpOverTcp

	// TcpConnection的构造函数中，创建的IOEvent事件mTcpConnIOEvent设置读的回调函数TcpConnection::readCallback是进行rtsp数据传输的关键

	// 对已建立的连接connfd的处理，建立一个rtsp连接。
	// mTcpConnIOEvent事件发生的时候实际就是在传输数据，而触发事件mTriggerEvent是断开连接的
	RtspConnection *conn = RtspConnection::createNew(this, connfd);

	/*
	2.
	设置单个RtspConnection关闭连接的回调函数，在TcpConnection::handleRead处理读事件客户端关闭连接或读取失败及RtspConnection::handleReadBytes进行option解析错误的时候调用，把要断开连接的connfd加入到要断开连接的队列中，并添加断开连接的触发事件mTriggerEvent。
	*/
	conn->setDisconnectionCallback(RtspServer::disconnectionCallback, this);

	// 3. 这里把新连接的connfd和对应的RtspConnection* conn加入到已建立连接的map中mConnections
	mConnections.insert(std::make_pair(connfd, conn));
}

/*---------------客户端关闭连接或读取错误或option解析错误处理单个连接----------------------*/

// 在TcpConnection::handleRead处理读事件客户端关闭连接或读取失败及RtspConnection::handleReadBytes进行option解析错误的时候，会调用该回调函数TcpConnection::handleDisconnection处理这个连接，把要取消连接的sockfd加入到队列mDisconnectionlist，并添加触发事件mTriggerEvent到std::vector<TriggerEvent*>
// mTriggerEvents，稍后处理断开连接。

void RtspServer::disconnectionCallback(void *arg, int sockfd) {
	// 处理断开连接的回调函数
	RtspServer *rtspServer = (RtspServer *)arg;
	rtspServer->handleDisconnection(sockfd);
}

void RtspServer::handleDisconnection(int sockfd) {
	std::lock_guard<std::mutex> lguard(m_mutex);

	// 要取消连接的描述符加入到队列mDisconnectionlist，建立连接的时候是存在map<int, RtspConnection*>
	// mConnections
	mDisconnectionlist.push_back(sockfd);
	// 添加触发事件mTriggerEvent到std::vector<TriggerEvent*> mTriggerEvents，稍后处理断开连接
	mEnv->scheduler()->addTriggerEvent(mTriggerEvent);
}

/*-----mTriggerEvent事件发生的时候调用，释放RtspConnection内存并从建立连接的映射删除--------*/

void RtspServer::triggerCallback(void *arg) {
	RtspServer *rtspServer = (RtspServer *)arg;
	// printf("triggerCallback回调\n");
	rtspServer->handleDisconnectionList();
}

void RtspServer::handleDisconnectionList() {
	std::lock_guard<std::mutex> lguard(m_mutex);
	// 遍历mDisconnectionlist所有要关闭的连接描述符，从存储连接描述符和RtspConnection的map
	// mConnections中找到对应的RtspConnection进行释放，然后删除对应的fd
	for (std::vector<int>::iterator it = mDisconnectionlist.begin(); it != mDisconnectionlist.end();
		 ++it) {
		int sockfd = *it;
		std::map<int, RtspConnection *>::iterator _it = mConnections.find(sockfd);
		assert(_it != mConnections.end());
		Delete::release(_it->second); // 释放资源，包括关闭连接
		mConnections.erase(sockfd);
	}
	mDisconnectionlist.clear();
}

// 向mMediaSessions添加媒体会话
bool RtspServer::addMeidaSession(MediaSession *mediaSession) {

	if (mMediaSessions.find(mediaSession->name()) != mMediaSessions.end())
		return false;

	mMediaSessions.insert(std::make_pair(mediaSession->name(), mediaSession));

	return true;
}

MediaSession *RtspServer::loopupMediaSession(std::string name) {
	// 根据名称查找媒体会话
	auto it = mMediaSessions.find(name);
	if (it == mMediaSessions.end())
		return nullptr;

	return it->second;
}

// 获取媒体会话的URL
std::string RtspServer::getUrl(MediaSession *session) {
	char url[200];

	snprintf(
		url, sizeof(url), "rtsp://%s:%d/%s", sockets::getLocalIp().c_str(), mAddr.getPort(),
		session->name().c_str());

	return std::string(url);
}

RtspServer::~RtspServer() {
	Delete::release(mTriggerEvent);
}
