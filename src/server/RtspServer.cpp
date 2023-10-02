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
    TcpServer(env, addr)
{

    // TcpServer完成了连接

    /*
    // RtspServer->TcpServer->Acceptor(sockets::createTcpSock())

    1. Acceptor创建rtsp server的TCP套接字，绑定好指定的端口，作为RTSP服务器的监听套接字,然后创建一个接收连接的IO事件mAcceptIOEvent，设置mAcceptIOEvent可读事件接受新连接的回调函数. 当有一个连接过来向rtsp server发起请求,触发可读事件进入Acceptor::readCallback回调函数进行处理。回调函数accept接收客户端连接返回一个新的套接字connfd进行服务端和客户端通信, 然后handleNewConnection
    
    通过多态的回调函数回RtspServer::handleNewConnection处理新连接 
    mEnv->scheduler()->addIOEvent(mTcpConnIOEvent);
    
    2. TcpServer是rtspServer父类，TcpServer::handleNewConnection是个纯虚函数,所以newConnectionCallback调用的处理连接的虚函数其实是RtspServer::handleNewConnection进行处理,根据服务器和客户端通信的connfd创建一个新的rtsp连接,设置断开连接的对回调函数RtspServer::disconnectionCallback. 把要取消的连接加入到队列mDisconnectionlist
    
    3. 添加触发事件mTriggerEvent，以便稍后处理断开连接
    4. 创建互斥锁用于多线程同步

    */

    // 触发事件,触发的回调函数取调用执行对应的删除操作的函数,遍历所有要关闭的连接，取出来描述符进行关闭
    mTriggerEvent = TriggerEvent::createNew(this);

    mTriggerEvent->setTriggerCallback(RtspServer::triggerCallback);

    mMutex = Mutex::createNew();
}

// 这里开始和RtspConnection建立联系的桥梁
void RtspServer::handleNewConnection(int connfd)
{
    
    // 客户连接,处理服务器创建的通信的fd新连接
    // mEnv->scheduler()->addIOEvent(mTcpConnIOEvent);
    RtspConnection* conn = RtspConnection::createNew(this, connfd);

    // 设置断开连接处理的回调函数，关闭连接时操作
    conn->setDisconnectionCallback(RtspServer::disconnectionCallback, this);

    // 添加新的RtspConnection到连接容器,进行遍历处理
    // 根据服务端和客户端通信的fd（accept返回的）,通过与之对应的RtspConnection
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
