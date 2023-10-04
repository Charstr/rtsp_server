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

    // 到这里Acceptor创建了新的socket套接字、绑定的端口和IP但是没有进行监听listen和接受连接accept
    // 监听在server->start()时候进行;accept在进行事件调度的时候，当有新的连接过来了进行。
    // 设置的回调函数当mAcceptIOEvent事件触发的时候调用TcpServer::newConnectionCallback进行处理。
    // 这个回调的前置函数调用tcpServer->handleNewConnection，实际是通过多态调用的 RtspServer::handleNewConnection进行处理，也就是进入到了RtspServer相关处理过程中

    mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this);
}


void TcpServer::newConnectionCallback(void* arg, int connfd)
{
    // 处理新连接的回调函数
    TcpServer* tcpServer = (TcpServer*)arg;
    // 多态，这里调用的是RtspServer的
    tcpServer->handleNewConnection(connfd);
}


// rtspServer->start实际调用这个父类的函数，开始监听新连接
void TcpServer::start() {
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