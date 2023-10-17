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

    // mAcceptIOEvent用的是listenfd，只是接受连接，这里创建实际建立连接之后的用于传输的事件mTcpConnIOEvent，用的是connfd
    mTcpConnIOEvent = IOEvent::createNew(sockfd, this);
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

/*-----------------设置事件的读写权限------------*/
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


/*-------------读写错误的回调函数-----------------*/
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

/*---------------具体的处理各种事件所用的函数--------------*/
// epoll轮循到已建立连接的描述符connfd，对应的事件mTcpConnIOEvent发生，调用对应的回调函数
// 接下来需要根据客户端发来的信息进行各种方法的解析

void TcpConnection::handleRead() {

    // 从 connfd 中读取数据，并将存到inputbuffer 中，读取到数据已经完成的传输层的连接
    // 这里要进行应用层rtsp的连接，解析从客户端读取到的请求消息

    int ret = mInputBuffer.read(mSocket.fd());
    // 客户端断开连接或读取失败，那么这个连接就是要取消的
    // handleDisconnection调用回调函数把要断开的连接加入到队列mDisconnectionlist，并添加触发事件mTriggerEvent。当mTriggerEvent触发的时候，调用handleDisconnectionList函数遍历所有要关闭的连接描述符，取出来描述符进行关闭
    if(ret == 0) { // 客户端断开连接
        LOG_DEBUG("client disconnect\n");
        handleDisconnection();
        return;
    }else if(ret < 0){//读取失败
        LOG_ERROR("read err\n");
        handleDisconnection(); 
        return;
    }

    // 正常接收到客户端的请求数据，就要进行解析处理，这里是多态实现，调用的是RtspConnection的函数
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


// 处理断开连接
void TcpConnection::handleDisconnection() {
    if(TcpConnection::mDisconnectionCallback)
        TcpConnection::mDisconnectionCallback(mArg, mSocket.fd());
}