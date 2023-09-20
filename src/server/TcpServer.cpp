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
    mAcceptor->setNewConnectionCallback(newConnectionCallback, this);
    /*
    void Acceptor::setNewConnectionCallback(NewConnectionCallback cb, void* arg)
    {
        // 设置新连接回调函数和参数
        mNewConnectionCallback = cb;
        mArg = arg;
    }
    // 回调函数怎么执行？

    void Acceptor::handleRead() {
        if(mNewConnectionCallback)
            mNewConnectionCallback(mArg, connfd);
    }

    */




    
}

TcpServer::~TcpServer()
{
    //delete mAcceptor;
    Delete::release(mAcceptor);
}

void TcpServer::start()
{
    // 启动服务器开始监听新连接
    mAcceptor->listen();
}

void TcpServer::newConnectionCallback(void* arg, int connfd)
{
    // 处理新连接的回调函数
    TcpServer* tcpServer = (TcpServer*)arg;
    tcpServer->handleNewConnection(connfd);
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