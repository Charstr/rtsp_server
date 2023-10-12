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

    // 3. 创建接受连接的IO事件，将socket描述符传递给新的IO事件。
    mAcceptIOEvent = IOEvent::createNew(mSocket.fd(), this);
    
    /*
    4. 设置mAcceptIOEvent接受连接的回调函数Acceptor::readCallback。回调函数用socket的accept用函数接受连接返回一个已建立连接的server和客户端通信的connfd，然后调用处理用这个connfd处理连接的回调函数Acceptor::mNewConnectionCallback进行处理。

    5. 这里处理连接的回调函数在TcpServer的构造函数设置mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this)实际是通过多态调用rtspServer::newConnectionCallback进行处理

    6. mAcceptIOEvent添加到epoll是在server->start()调用Acceptor::listen时候发生，因为在listen之后才会有这样的事件发生，所以要晚些加入到调度器
    */ 
    mAcceptIOEvent->setReadCallback(Acceptor::readCallback);
    mAcceptIOEvent->enableReadHandling();
}

Acceptor::~Acceptor(){
    // 移除事件调度释放内存
    if(mListenning)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);
    Delete::release(mAcceptIOEvent);
}

// server->start
// 调用listen(),开启对mSocket的监听，同时把mAcceptIOEvent注册到EventScheduler
void Acceptor::listen() {
    // 开始监听连接请求，这个时候才会有连接过来的事件mAcceptIOEvent，所以这时候加入到调度器
    mSocket.listen(1024);
    mListenning = true;
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);
}


/*-------loop时候mAcceptIOEvent发生，调用这个函数处理进来的连接--------*/

void Acceptor::readCallback(void* arg) {
    Acceptor* acceptor = (Acceptor*)arg;
    // 调用处理函数
    acceptor->handleRead();
}

void Acceptor::handleRead() {
    
    // 接受连接请求，返回和外界通信的套接字描述符 
    int connfd = mSocket.accept();
    LOG_DEBUG("client connect: %d\n", connfd);
    // 调用处理连接的具体函数，这个函数在TcpServer的构造函数设置mAcceptor->setNewConnectionCallback(TcpServer::newConnectionCallback, this)，实际是通过多态调用rtspServer::newConnectionCallback进行处理
    if(Acceptor::mNewConnectionCallback)
        Acceptor::mNewConnectionCallback(mArg, connfd);
}


