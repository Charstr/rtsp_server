#ifndef _TCP_SERVER_H_
#define _TCP_SERVER_H_
#include <map>

#include "Acceptor.h"
#include "schedule/UsageEnvironment.h"
#include "InetAddress.h"
#include "TcpConnection.h"

//class TcpConnection;

// TcpServer是一个通用的TCP服务器类，用于监听新连接。
class TcpServer
{
public:
    virtual ~TcpServer();

    void start(); // 启动服务器

protected:
    // 处理新连接
    TcpServer(UsageEnvironment* env, const Ipv4Address& addr);
    virtual void handleNewConnection(int connfd) = 0;
    //virtual void handleDisconnection(int sockfd);

private:
    static void newConnectionCallback(void* arg, int connfd);
    //static void disconnectionCallback(void* arg, int sockfd);

protected:
    UsageEnvironment* mEnv;
    Acceptor* mAcceptor;
    Ipv4Address mAddr;
//    std::map<int, TcpConnection*> mTcpConnections;
};

#endif //_TCP_SERVER_H_