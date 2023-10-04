#include "TcpConnection.h"
#include "SocketsOps.h"
#include "base/Logging.h"
#include "base/New.h"

#include <unistd.h>
#include <stdlib.h>


// TCP连接的实现，包含了TCP连接的读写处理、错误处理以及断开连接的回调处理等功能。
TcpConnection::TcpConnection(UsageEnvironment* env, int sockfd) :
    mEnv(env),
    mSocket(sockfd), // 这里是accept后返回的已建立连接的connfd
    mDisconnectionCallback(NULL),
    mArg(NULL) {

    // 每个mTcpConnIOEvent都是一个channel
    // 创建了一个IOEvent对象，并设置了读写和错误的回调函数，
    mTcpConnIOEvent = IOEvent::createNew(sockfd, this);

    mTcpConnIOEvent->setReadCallback(TcpConnection::readCallback);
    mTcpConnIOEvent->setWriteCallback(TcpConnection::writeCallback);
    mTcpConnIOEvent->setErrorCallback(TcpConnection::errorCallback);

    //默认只开启读事件，因为rtsp服务器一般只分发数据给客户端
    mTcpConnIOEvent->enableReadHandling(); 

    // 处理连接的IO事件加入到事件调度器
    mEnv->scheduler()->addIOEvent(mTcpConnIOEvent);
}

TcpConnection::~TcpConnection()
{
    mEnv->scheduler()->removeIOEvent(mTcpConnIOEvent);
    //delete mTcpConnIOEvent;
    Delete::release(mTcpConnIOEvent);
}

void TcpConnection::setDisconnectionCallback(DisconnectionCallback cb, void* arg)
{
    mDisconnectionCallback = cb;
    mArg = arg;
}

void TcpConnection::enableReadHandling()
{
    if(mTcpConnIOEvent->isReadHandling())
        return;
    
    mTcpConnIOEvent->enableReadHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::enableWriteHandling()
{
    if(mTcpConnIOEvent->isWriteHandling())
        return;
    
    mTcpConnIOEvent->enableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::enableErrorHandling()
{
    if(mTcpConnIOEvent->isErrorHandling())
        return;

    mTcpConnIOEvent->enableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::disableReadeHandling()
{
    if(!mTcpConnIOEvent->isReadHandling())
        return;

    mTcpConnIOEvent->disableReadeHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}   

void TcpConnection::disableWriteHandling()
{
    if(!mTcpConnIOEvent->isWriteHandling())
        return;

    mTcpConnIOEvent->disableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::disableErrorHandling()
{
    if(!mTcpConnIOEvent->isErrorHandling())
        return;

    mTcpConnIOEvent->disableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::handleRead() {
    // 负责处理TCP连接的可读事件,从 connfd 中读取数据，并将其放入 inputbuffer 中
    int ret = mInputBuffer.read(mSocket.fd());

    if(ret == 0) {
        LOG_DEBUG("client disconnect\n");
        handleDisconnection(); // 客户端断开连接
        return;
    }else if(ret < 0){
        LOG_ERROR("read err\n");
        handleDisconnection(); //读取失败
        return;
    }

    // 多态调用rtspconnection对象的函数
    handleReadBytes();
}

void TcpConnection::handleReadBytes()
{
    LOG_DEBUG("default read handle\n");
    mInputBuffer.retrieveAll();
}

void TcpConnection::handleWrite()
{
    LOG_DEBUG("default wirte handle\n");
    mOutBuffer.retrieveAll();
}

void TcpConnection::handleError()
{
    LOG_DEBUG("default error handle\n");
}

void TcpConnection::readCallback(void* arg)
{
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleRead();
}

void TcpConnection::writeCallback(void* arg)
{
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleWrite();
}

void TcpConnection::errorCallback(void* arg)
{
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleError();
}

// 处理断开连接也是rtsp进行的操作
void TcpConnection::handleDisconnection() {
    if(TcpConnection::mDisconnectionCallback)
        TcpConnection::mDisconnectionCallback(mArg, mSocket.fd());
}