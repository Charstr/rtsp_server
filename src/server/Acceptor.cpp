#include "Acceptor.h"
#include "server/SocketsOps.h"
#include "base/Logging.h"
#include "base/New.h"

Acceptor* Acceptor::createNew(UsageEnvironment* env, const Ipv4Address& addr)
{
    //return new Acceptor(env, addr);
    return New<Acceptor>::allocate(env, addr);
}

// mSocket是封装的socket类，构造函数结束后，监听 socket 已经建立好，并已绑定到对应地址和端口了

Acceptor::Acceptor(UsageEnvironment* env, const Ipv4Address& addr) :
    mEnv(env),
    mAddr(addr),
    mSocket(sockets::createTcpSock()), // 创建监听的socket套接字mSocket.fd();
    mNewConnectionCallback(NULL)
{
    // 1. 允许地址重用，一个服务突然关掉，然后再重启，短时间内端口可能还没有释放，这样端口能重复利用
    mSocket.setReuseAddr(1);
    // 2. 绑定套接字到server地址和端口
    mSocket.bind(mAddr);

    // 3. 创建接受连接的IO事件，将socket描述符传递给新的IO事件。对应muduo的Acceptor::acceptChannel_
    mAcceptIOEvent = IOEvent::createNew(mSocket.fd(), this);
    
    /*
    4. 把回调函数Acceptor::readCallback注册到mAcceptIOEvent也就是这个接受连接的事件。当事件调度器检测到有新的可读事件（新的连接）过来，回调函数accept用函数接受连接返回一个server和客户端通信的fd，然后调用Acceptor::mNewConnectionCallback函数处理新连接。这个函数设置的是TcpServer::newConnectionCallback，通过多态，调用rtspServer::newConnectionCallback进行处理。

    */ 
    mAcceptIOEvent->setReadCallback(Acceptor::readCallback);
    mAcceptIOEvent->enableReadHandling();
}

Acceptor::~Acceptor(){
    // 移除事件调度释放内存
    if(mListenning)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);

    //delete mAcceptIOEvent;
    Delete::release(mAcceptIOEvent);
}

// Acceptor相当于是只接受连接，处理新连接的函数是TcpServer干的事情，具体是通过多态分配到rtsp等
void Acceptor::setNewConnectionCallback(NewConnectionCallback cb, void* arg){
    // 设置新连接回调函数和参数
    mNewConnectionCallback = cb;
    mArg = arg;
}

// server->start
// 调用listen(),开启对mSocket的监听,同时让mAcceptIOEvent注册到EventScheduler的事件监听器上.
void Acceptor::listen() {

    // 开始监听连接请求
    mListenning = true;
    mSocket.listen(1024);

    // 将接受连接的IO事件mAcceptIOEvent添加到事件调度器的循环中，这个为什么是add而不是update
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);
}


// loop时候进来连接的处理
void Acceptor::readCallback(void* arg) {
    // 有连接请求时触发可读事件
    Acceptor* acceptor = (Acceptor*)arg;
    // 调用处理函数
    acceptor->handleRead();
}

void Acceptor::handleRead() {
    
    // 接受连接请求，返回和外界通信的套接字描述符 
    int connfd = mSocket.accept();
    LOG_DEBUG("client connect: %d\n", connfd);
    // 调用处理连接的具体函数
    if(Acceptor::mNewConnectionCallback)
        Acceptor::mNewConnectionCallback(mArg, connfd);
}


