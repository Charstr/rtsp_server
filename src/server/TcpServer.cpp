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
    // 到这里Acceptor创建了新的socket套接字、绑定的端口和IP但是没有进行监听listen和接受连接accept。监听在server->start()时候进行;accept在进行事件调度的时候，当有新的连接过来了进行。
    
    assert(mAcceptor);

    /*
    1. Acceptor用于接受新的连接，会创建一个新的tcp连接mSocket，设置地址重用和绑定，创建接受连接的mAcceptIOEvent事件，当mAcceptIOEvent事件触发也就是有新的连接过来的时候，调用回调函数Acceptor::readCallback，使用处理函数TcpServer::newConnectionCallback，通过多态RtspServer::handleNewConnection进行处理，进入到了RtspServer相关处理过程中
    */
    // 这里设置接收连接的回调函数，通过多态进行rtsp的处理
    mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this);
    // 这里类似于muduo中的TcpServer::TcpServer构造函数中的acceptor_->setNewConnectionCallback

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