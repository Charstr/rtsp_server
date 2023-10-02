#include <assert.h>

#include "TcpServer.h"
#include "base/Logging.h"
#include "base/New.h"

TcpServer::TcpServer(UsageEnvironment* env, const Ipv4Address& addr) :
    mEnv(env),
    mAddr(addr)
{
    // 创建Acceptor，用于监听新连接
    mAcceptor = Acceptor::createNew(env, addr);
    assert(mAcceptor);

    // Acceptor完成了和客户端的连接
    // 这里因为是建立连接之前的过程,还没有连接过来之前进行的设置，相当于是个环
    // Acceptor里边完成了连接handleRead要处理读，使用的回调函数mNewConnectionCallback在TcpServer
    // 也就是这里设置
    mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this);

    // newConnectionCallback是设置的处理新连接的回调函数，调用处理函数handleNewConnection进行处理
    // TcpServer是rtspServer父类，TcpServer::handleNewConnection是个纯虚函数,所以
    // newConnectionCallback调用的处理连接的虚函数其实是RtspServer::handleNewConnection进行处理的
    // 同样的，这里也是在真正建立连接之前设置的,当触发可读事件的时候，会进行的一系列的调用
}


void TcpServer::newConnectionCallback(void* arg, int connfd)
{
    // 处理新连接的回调函数
    TcpServer* tcpServer = (TcpServer*)arg;
    // 多态，这里调用的是RtspServer的
    tcpServer->handleNewConnection(connfd);
}


void TcpServer::start()
{
    // 启动服务器开始监听新连接
    mAcceptor->listen();
}

TcpServer::~TcpServer()
{
    //delete mAcceptor;
    Delete::release(mAcceptor);
}


#if 0
void TcpServer::handleNewConnection(int connfd)
{
    TcpConnection* tcpConn = TcpConnection::createNew(mEnv, connfd);
    tcpConn->setDisconnectionCallback(disconnectionCallback, this);
    mTcpConnections.insert(std::make_pair(connfd, tcpConn));
}

void TcpServer::disconnectionCallback(void* arg, int sockfd)
{
    TcpServer* tcpServer = (TcpServer*)arg;
    tcpServer->handleDisconnection(sockfd);
}

void TcpServer::handleDisconnection(int sockfd)
{
    std::map<int, TcpConnection*>::iterator it = mTcpConnections.find(sockfd);
    if(it == mTcpConnections.end())
    {
        LOG_DEBUG("can't find\n");
        return;
    }
    
    delete it->second; //释放内存，析构函数会删除IO事件，释放内存，socket生命结束关闭文件描述符
    mTcpConnections.erase(sockfd);
}
#endif