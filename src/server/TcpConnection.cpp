#include "TcpConnection.h"
#include "SocketsOps.h"
#include "base/Logging.h"
#include "base/New.h"

#include <unistd.h>
#include <stdlib.h>


// TCP连接的实现，包含了TCP连接的读写处理、错误处理以及断开连接的回调处理等功能。
TcpConnection::TcpConnection(UsageEnvironment* env, int sockfd) :
    mEnv(env),
    mSocket(sockfd), // accept后返回的已建立连接的connfd
    mDisconnectionCallback(NULL),
    mArg(NULL) 
{

    // 创建实际建立连接之后的用于传输的事件mTcpConnIOEvent
    mTcpConnIOEvent = IOEvent::createNew(sockfd, this);
    // 传输数据的回调
    mTcpConnIOEvent->setReadCallback(TcpConnection::readCallback);
    mTcpConnIOEvent->setWriteCallback(TcpConnection::writeCallback);
    mTcpConnIOEvent->setErrorCallback(TcpConnection::errorCallback);

    //默认只开启读事件，因为rtsp服务器一般只分发数据给客户端
    mTcpConnIOEvent->enableReadHandling(); 

    // 处理连接的IO事件加入到事件调度器
    mEnv->scheduler()->addIOEvent(mTcpConnIOEvent);
}

TcpConnection::~TcpConnection() {
    mEnv->scheduler()->removeIOEvent(mTcpConnIOEvent);
    Delete::release(mTcpConnIOEvent);
}


void TcpConnection::enableReadHandling(){
    if(mTcpConnIOEvent->isReadHandling())
        return;
    
    mTcpConnIOEvent->enableReadHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::enableWriteHandling(){
    if(mTcpConnIOEvent->isWriteHandling())
        return;
    
    mTcpConnIOEvent->enableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::enableErrorHandling(){
    if(mTcpConnIOEvent->isErrorHandling())
        return;

    mTcpConnIOEvent->enableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::disableReadeHandling(){
    if(!mTcpConnIOEvent->isReadHandling())
        return;

    mTcpConnIOEvent->disableReadeHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}   

void TcpConnection::disableWriteHandling(){
    if(!mTcpConnIOEvent->isWriteHandling())
        return;

    mTcpConnIOEvent->disableWriteHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::disableErrorHandling() {
    if(!mTcpConnIOEvent->isErrorHandling())
        return;

    mTcpConnIOEvent->disableErrorHandling();
    mEnv->scheduler()->updateIOEvent(mTcpConnIOEvent);
}

void TcpConnection::handleRead() {

    // 负责处理TCP连接的可读事件,从 connfd 中读取数据，并将其放入 inputbuffer 中
    // 这里的读取数据其实是接受客户端发过来的option请求
    int ret = mInputBuffer.read(mSocket.fd());

    if(ret == 0) {
        LOG_DEBUG("client disconnect\n");
        // 客户端断开连接，多态调用断开连接的函数是RtspServer::disconnectionCallback
        // 把要取消的连接加入到队列,添加触发事件mTriggerEvent，稍后处理断开连接
        // 当mTriggerEvent触发的时候，调用handleDisconnectionList函数遍历所有要关闭的连接描述符，取出来描述符进行关闭
        handleDisconnection();// 如果某个连接处理失败
        return;
    }else if(ret < 0){
        LOG_ERROR("read err\n");
        handleDisconnection(); //读取失败
        return;
    }

    // 读取到数据，就多态调用RtspConnection::handleReadBytes进行字节的解析
    handleReadBytes();
}

void TcpConnection::handleReadBytes(){
    LOG_DEBUG("default read handle\n");
    mInputBuffer.retrieveAll();
}

// mOutBuffer没数据时候不需要向socket 中写入数据，但是此时 socket 一直是处于可写状态的， 这将会导致 TcpConnection::handleWrite() 一直被触发。
void TcpConnection::handleWrite(){
    LOG_DEBUG("default wirte handle\n");
    mOutBuffer.retrieveAll();
}

void TcpConnection::handleError(){
    LOG_DEBUG("default error handle\n");
}

void TcpConnection::readCallback(void* arg){
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleRead();
}

void TcpConnection::writeCallback(void* arg){
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleWrite();
}

void TcpConnection::errorCallback(void* arg){
    TcpConnection* tcpConnection = (TcpConnection*)arg;
    tcpConnection->handleError();
}

// 处理断开连接
void TcpConnection::handleDisconnection() {
    if(TcpConnection::mDisconnectionCallback)
        TcpConnection::mDisconnectionCallback(mArg, mSocket.fd());
}