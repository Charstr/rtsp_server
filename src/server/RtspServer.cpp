#include <algorithm>
#include <assert.h>
#include <stdio.h>

#include "RtspServer.h"
#include "base/New.h"

RtspServer* RtspServer::createNew(UsageEnvironment* env, Ipv4Address& addr)
{
    //return new RtspServer(env, addr);
    return New<RtspServer>::allocate(env, addr);
}

RtspServer::RtspServer(UsageEnvironment* env, const Ipv4Address& addr) :
    TcpServer(env, addr) {

    /*
    TcpServer部分：

    1. Acceptor接受了连接交给TcpServer进行处理，实际进行处理的是通过多态调用的RtspServer。
    2. 创建连接socket并绑定地址和端口后会有一个mAcceptIOEvent事件，这个事件会在listen监听时候加入到调度的epoll中。

    3. 接下里当rtspserver->start启动后，回accept接受连接，返回进行通信的connfd，当轮询到该事件触发的时候用设置的回调函数Acceptor::mNewConnectionCallback进行处理，该回调函数是在tcpserver中设置的 TcpServer::newConnectionCallback，调用tcpServer->handleNewConnection，实际是通过多态调用的 RtspServer::handleNewConnection进行处理。到这里上边的TcpServer函数完成了。

    4. handleNewConnection会根据已经建立连接的connfd,创建RtspConnection对象，并设置处理断开连接时候的回调函数RtspServer::disconnectionCallback把该connfd加入到要断开连接的队列中，并向调度器添加触发事件，稍后处理断开连接，接下来把<connfd, RtspConnection>的对应映射加入到mConnections。

    4.1 创建RtspConnection对象时候，其中创建一个TcpConnection，构造函数中创建了IOEvent事件mTcpConnIOEvent，设置读（TcpConnection::readCallback）写异常回调函数，默认启用只读事件，然后把mTcpConnIOEvent添加到EventScheduler，获取客户端IPmPeerIp。当事件mTcpConnIOEvent发生的时候，会调用回调函数TcpConnection::readCallback进行数据的处理
    
    */
    
    // 5. 上边4创建了 connfd和RtspConnection的对应映射加入到了mConnections，并且设置了RtspConnection断开连接的回调函数disconnectionCallback，回调函数把connfd加入到了需要断开连接的队列mDisconnectionlist中，并把断开连接的触发事件mTriggerEvent加入到调度器中，通过回调的触发事件mTriggerEvent进行触发删除。
    // 疑问：单个连接为什么RtspConnection

    // 6. 接下来创建触发事件，并设置触发事件的回调函数triggerCallback，回调函数遍历所有要关闭的连接描述符，从mConnections找到对应的RtspConnection释放内存关闭资源，然后从mConnections中删除对应的connfd的值。最后把map清空。

    mTriggerEvent = TriggerEvent::createNew(this);
    mTriggerEvent->setTriggerCallback(RtspServer::triggerCallback);

    // 7. 创建互斥锁用于多线程同步
    mMutex = Mutex::createNew();
}

// 有新连接进来的时候调用，处理连接的回调函数在TcpServer中设置回调函数，mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this);调用tcpServer->handleNewConnection，实际是通过多态调用的 RtspServer::handleNewConnection进行处理。也就是说，accept接受了连接之后，通过tcpserver传递给了RtspServer处理新连接，创建与之对应的RtspConnection进行数据的传输工作

// 对应于muduo的TcpServer::newConnection
void RtspServer::handleNewConnection(int connfd) {

    // connfd也就是listen之后得到的socketfd也就是已经建立的连接，用来和客户端进行通信的fd，所有的rtsp数据的发送都是通过这个connfd进行的。

    // 1. 根据已经建立连接的connfd,创建RtspConnection对象。其中创建一个TcpConnection，构造函数中创建了IOEvent事件mTcpConnIOEvent，设置读写异常回调函数，默认启用只读事件，add到EventScheduler，获取客户端IPmPeerIp。mTcpConnIOEvent是数据传输的事件
    // 设置了是否mIsRtpOverTcp
    RtspConnection* conn = RtspConnection::createNew(this, connfd);

    // 2. 设置RtspConnection关闭单个连接的回调函数，把当前连接的fd加入到要断开连接的队列中，并
    // 添加该触发事件
    conn->setDisconnectionCallback(RtspServer::disconnectionCallback, this);

    // 3. 加入到已建立连接的map中mConnections，处理数据时候对这个进行处理
    // 对应于TcpServer::connections_
    mConnections.insert(std::make_pair(connfd, conn));
}

// 对于一个mAcceptIOEvent处理连接的进行的调用
void RtspServer::disconnectionCallback(void* arg, int sockfd){
    // 处理断开连接的回调函数
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleDisconnection(sockfd);
}

// 断开连接的回调函数在这里加入到触发事件
void RtspServer::handleDisconnection(int sockfd){
    MutexLockGuard mutexLockGuard(mMutex);
    // 把要取消的连接加入到队列
    mDisconnectionlist.push_back(sockfd); 
    // 添加触发事件mTriggerEvent到std::vector<TriggerEvent*> mTriggerEvents，稍后处理断开连接
    mEnv->scheduler()->addTriggerEvent(mTriggerEvent);
}

// mTriggerEvent调用
void RtspServer::triggerCallback(void* arg){
    // 处理断开连接列表的回调函数
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleDisconnectionList();
}

void RtspServer::handleDisconnectionList(){
    MutexLockGuard mutexLockGuard(mMutex);

    // 遍历所有要关闭的连接描述符，取出来描述符进行关闭
    for(std::vector<int>::iterator it = mDisconnectionlist.begin(); it != mDisconnectionlist.end(); ++it) {
        int sockfd = *it;
        std::map<int, RtspConnection*>::iterator _it = mConnections.find(sockfd);
        assert(_it != mConnections.end());
        Delete::release(_it->second);// 释放资源，包括关闭连接
        mConnections.erase(sockfd);
    }

    mDisconnectionlist.clear();
}


bool RtspServer::addMeidaSession(MediaSession* mediaSession){
    // 向媒体会话容器添加媒体会话
    if(mMediaSessions.find(mediaSession->name()) != mMediaSessions.end())
        return false;

    mMediaSessions.insert(std::make_pair(mediaSession->name(), mediaSession));

    return true;
}

MediaSession* RtspServer::loopupMediaSession(std::string name){
    // 根据名称查找媒体会话
    std::map<std::string, MediaSession*>::iterator it = mMediaSessions.find(name);
    if(it == mMediaSessions.end())
        return NULL;
    
    return it->second;
}

// 获取媒体会话的URL
std::string RtspServer::getUrl(MediaSession* session) {
    char url[200];

    snprintf(url, sizeof(url), "rtsp://%s:%d/%s", sockets::getLocalIp().c_str(),
                mAddr.getPort(), session->name().c_str());

    return std::string(url);
}


RtspServer::~RtspServer() {
    Delete::release(mTriggerEvent);
    Delete::release(mMutex);
}
