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
    1. Acceptor接受了连接交给TcpServer进行处理，实际进行处理的是通过多态调用的RtspServer。
    2. 创建连接socket并绑定地址和端口后会有一个mAcceptIOEvent事件，这个事件会在listen监听时候加入到调度的epoll中。
    3. 接下里当rtspserver->start启动后，回accept接受连接，返回进行通信的connfd，当轮询到该事件触发的时候用设置的回调函数Acceptor::mNewConnectionCallback进行处理，该回调函数是在tcpserver中设置的 TcpServer::newConnectionCallback，调用tcpServer->handleNewConnection，实际是通过多态调用的 RtspServer::handleNewConnection进行处理。
  
    3. 添加触发事件mTriggerEvent，触发的回调函数取调用执行对应的删除操作的函数,遍历所有要关闭的连接，取出来描述符进行关闭
    4. 创建互斥锁用于多线程同步

    */

    // 创建断开连接的触发事件
    mTriggerEvent = TriggerEvent::createNew(this);
    // 触发事件，遍历所有的mConnections
    mTriggerEvent->setTriggerCallback(RtspServer::triggerCallback);

    mMutex = Mutex::createNew();
}

// mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this);
// TcpServer::newConnectionCallback，调用tcpServer->handleNewConnection，实际是通过多态调用的 RtspServer::handleNewConnection进行处理。

// 也就是说，accept接受了连接之后，通过tcpserver传递给了RtspServer进行新连接进来的处理

// 这里开始和RtspConnection建立联系的桥梁
void RtspServer::handleNewConnection(int connfd) {

    /*
    connfd也就是listen之后得到的socketfd也就是已经建立的连接，用来和客户端进行通信的fd，所有的rtsp数据的发送都是通过这个connfd进行的。

    1. 创建一个TcpConnection，构造函数中创建了IOEvent事件mTcpConnIOEvent，设置可读的回调函数并add到EventScheduler，mTcpConnIOEvent就是用来进行数据传输的？
    */
    RtspConnection* conn = RtspConnection::createNew(this, connfd);

    // 这里设置了断开连接处理的回调函数，关闭连接时操作
    conn->setDisconnectionCallback(RtspServer::disconnectionCallback, this);

    // 服务端和客户端通信的connfd（accept返回的）以及对应的RtspConnection
    mConnections.insert(std::make_pair(connfd, conn));
}

void RtspServer::disconnectionCallback(void* arg, int sockfd)
{
    // 处理断开连接的回调函数
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleDisconnection(sockfd);
}

void RtspServer::handleDisconnection(int sockfd)
{
    MutexLockGuard mutexLockGuard(mMutex);
    // 把要取消的连接加入到队列
    mDisconnectionlist.push_back(sockfd); 
    // 添加触发事件，稍后处理断开连接
    mEnv->scheduler()->addTriggerEvent(mTriggerEvent);
}

void RtspServer::triggerCallback(void* arg)
{
    // 处理断开连接列表的回调函数
    RtspServer* rtspServer = (RtspServer*)arg;
    rtspServer->handleDisconnectionList();
}

void RtspServer::handleDisconnectionList()
{
    MutexLockGuard mutexLockGuard(mMutex);

    // 遍历所有要关闭的连接，取出来描述符进行关闭
    for(std::vector<int>::iterator it = mDisconnectionlist.begin(); it != mDisconnectionlist.end(); ++it)
    {
        int sockfd = *it;
        std::map<int, RtspConnection*>::iterator _it = mConnections.find(sockfd);
        assert(_it != mConnections.end());
        //delete _it->second;
        Delete::release(_it->second);// 释放资源，包括关闭连接
        mConnections.erase(sockfd);
    }

    mDisconnectionlist.clear();
}

bool RtspServer::addMeidaSession(MediaSession* mediaSession)
{
    // 向媒体会话容器添加媒体会话
    if(mMediaSessions.find(mediaSession->name()) != mMediaSessions.end())
        return false;

    mMediaSessions.insert(std::make_pair(mediaSession->name(), mediaSession));

    return true;
}

MediaSession* RtspServer::loopupMediaSession(std::string name)
{
    // 根据名称查找媒体会话
    std::map<std::string, MediaSession*>::iterator it = mMediaSessions.find(name);
    if(it == mMediaSessions.end())
        return NULL;
    
    return it->second;
}

// 获取媒体会话的URL
std::string RtspServer::getUrl(MediaSession* session)
{
    char url[200];

    snprintf(url, sizeof(url), "rtsp://%s:%d/%s", sockets::getLocalIp().c_str(),
                mAddr.getPort(), session->name().c_str());

    return std::string(url);
}




RtspServer::~RtspServer()
{
    //delete mTriggerEvent;
    //delete mMutex;

    Delete::release(mTriggerEvent);
    Delete::release(mMutex);
}
