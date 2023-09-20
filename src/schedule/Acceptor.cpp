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
    mSocket(sockets::createTcpSock()),
    mNewConnectionCallback(NULL)
{
    // 允许地址重用
    // 一个服务突然关掉，然后再重启，短时间内端口可能还没有释放，这样保证端口能重复利用
    mSocket.setReuseAddr(1);
    // 绑定套接字到指定地址
    mSocket.bind(mAddr);
    // 创建接受连接的IO事件,传递rtsp server的描述符
    // 将socket描述符传递给新的IO事件
    mAcceptIOEvent = IOEvent::createNew(mSocket.fd(), this);

    // 设置可读回调函数，有个连接向rtsp server发起请求，触发可读事件,回调函数回到Acceptor
    mAcceptIOEvent->setReadCallback(readCallback);
    // 启用读事件处理
    mAcceptIOEvent->enableReadHandling();
}

Acceptor::~Acceptor()
{
    if(mListenning)
        mEnv->scheduler()->removeIOEvent(mAcceptIOEvent);

    //delete mAcceptIOEvent;
    Delete::release(mAcceptIOEvent);
}

void Acceptor::listen()
{
    // 开始监听连接请求
    mListenning = true;
    mSocket.listen(1024);
    // 将接受连接的IO事件添加到事件调度器的循环中
    mEnv->scheduler()->addIOEvent(mAcceptIOEvent);
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb, void* arg)
{
    // 设置新连接回调函数和参数
    mNewConnectionCallback = cb;
    mArg = arg;
}

void Acceptor::readCallback(void* arg)
{
    // 有连接请求时触发可读事件
    Acceptor* acceptor = (Acceptor*)arg;
    // 通过回调函数
    acceptor->handleRead();
}

void Acceptor::handleRead()
{
    // 接受连接请求，返回连接的套接字描述符 
    int connfd = mSocket.accept();
    LOG_DEBUG("client connect: %d\n", connfd);

    // rtsp server来了一个可读事件，有一个新的描述符，就需要创建一个连接，
    // 同时设置一个取消连接时候的回调函数

    // 对正常的连接创建一个RtspConnection
    // 如果设置了新连接回调函数，则调用该函数处理新连接
    // RtspServer::handleNewConnection
    // RtspConnection* conn = RtspConnection::createNew(this, connfd);
    if(mNewConnectionCallback)
        mNewConnectionCallback(mArg, connfd);
}
