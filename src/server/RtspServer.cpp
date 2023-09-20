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
    // RtspServer->TcpServer->Acceptor
    /*
    1. TcpServer是来了一个新的连接就构造一个新的Acceptor接收. 
    1.1 TcpServer构造函数部分先是构造一个新的Acceptor. Acceptor构造函数创建一个rtsp socket mSocket, 创建接受连接的IO事件mAcceptIOEvent,将mSocket的描述符传递给新的IO事件mAcceptIOEvent,然后设置IO处理可读事件的回调函数readCallback,readCallback accept接收新的连接返回一个新的描述符connfd
    1.2 TcpServer构造函数部分,接下来设置处理新连接的回调函数newConnectionCallback. 然后利用多态(RtspServer继承TcpServer),运行RtspServer的函数handleNewConnection. 创建一个RtspConnection实例处理新连接,设置断开连接回调函数RtspServer::disconnectionCallback.
    
    */

    // 创建触发事件，用于处理断开连接
    mTriggerEvent = TriggerEvent::createNew(this);
    mTriggerEvent->setTriggerCallback(triggerCallback);

    mMutex = Mutex::createNew();
}

RtspServer::~RtspServer()
{
    //delete mTriggerEvent;
    //delete mMutex;

    Delete::release(mTriggerEvent);
    Delete::release(mMutex);
}

void RtspServer::handleNewConnection(int connfd)
{
    // 创建RtspConnection实例处理新连接
    RtspConnection* conn = RtspConnection::createNew(this, connfd);
    // 设置断开连接的回调函数，以触发关闭连接时的操作
    conn->setDisconnectionCallback(disconnectionCallback, this);
    // 添加新的RtspConnection到连接容器
    // int, RtspConnection*
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
    // 添加触发事件，以便稍后处理断开连接
    mEnv->scheduler()->addTriggerEvent(mTriggerEvent);
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