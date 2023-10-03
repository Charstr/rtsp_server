#include "Acceptor.h"
#include "server/SocketsOps.h"
#include "base/Logging.h"
#include "base/New.h"

Acceptor* Acceptor::createNew(UsageEnvironment* env, const Ipv4Address& addr)
{
    //return new Acceptor(env, addr);
    return New<Acceptor>::allocate(env, addr);
}

Acceptor::Acceptor(UsageEnvironment* env, const Ipv4Address& addr) :
    mEnv(env),
    mAddr(addr),
    mSocket(sockets::createTcpSock()), // 创建新的套接字
    mNewConnectionCallback(NULL)
{
    // 允许地址重用
    // 一个服务突然关掉，然后再重启，短时间内端口可能还没有释放，这样保证端口能重复利用
    mSocket.setReuseAddr(1);
    // 绑定套接字到指定server地址
    mSocket.bind(mAddr);

    // mAcceptIOEvent对应muduo的Acceptor::acceptChannel_

    // 创建接受连接的IO事件,传递rtsp server的描述符
    // 将socket描述符传递给新的IO事件
    mAcceptIOEvent = IOEvent::createNew(mSocket.fd(), this);
    
    // 把Acceptor::readCallback注册到mAcceptIOEvent
    // Acceptor::readCallback调用函数handleRead进行处理, accept函数接受连接返回一个server和客户端通信的fd
    
    mAcceptIOEvent->setReadCallback(Acceptor::readCallback);

    // 设置当前IO事件为可读事件
    // muduo中在Acceptor::listen函数中设置为何独
    mAcceptIOEvent->enableReadHandling();
}


void Acceptor::readCallback(void* arg)
{
    // 有连接请求时触发可读事件
    Acceptor* acceptor = (Acceptor*)arg;
    // 设置回调函数调用的处理函数
    acceptor->handleRead();
}

void Acceptor::handleRead()
{
    // 接受连接请求，返回和外界通信的套接字描述符 
    int connfd = mSocket.accept();
    LOG_DEBUG("client connect: %d\n", connfd);

    if(Acceptor::mNewConnectionCallback)
        Acceptor::mNewConnectionCallback(mArg, connfd);
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb, void* arg)
{
    // 设置新连接回调函数和参数
    mNewConnectionCallback = cb;
    mArg = arg;
}

// 调用listen(),开启对mSocket的监听,同时让mAcceptIOEvent注册到main EventScheduler的事件监听器上.

void Acceptor::listen() {

    // 开始监听连接请求
    mListenning = true;
    mSocket.listen(1024);
    // 将接受连接的IO事件添加到事件调度器的循环中
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);
}

Acceptor::~Acceptor()
{
    if(mListenning)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);

    //delete mAcceptIOEvent;
    Delete::release(mAcceptIOEvent);
}

