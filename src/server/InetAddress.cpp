#include "InetAddress.h"

Ipv4Address::Ipv4Address()
{

}

// 设置server端的网络地址和端口号
// "0.0.0.0"表示服务器绑定到所有可用网络接口的特定端口上，接受来自任何来源IP地址的连接。
Ipv4Address::Ipv4Address(std::string ip, uint16_t port) :
    mIp(ip),
    mPort(port)
{
    mAddr.sin_family = AF_INET;		  
    // 将点分十进制的IPv4地址字符串转换为一个 32 位的无符号整数（网络字节序），以便在套接字编程中使用
    mAddr.sin_addr.s_addr = inet_addr(ip.c_str()); 
    mAddr.sin_port = htons(port);
}

void Ipv4Address::setAddr(std::string ip, uint16_t port)
{
    mIp = ip;
    mPort = port;
    mAddr.sin_family = AF_INET;		  
    mAddr.sin_addr.s_addr = inet_addr(ip.c_str()); 
    mAddr.sin_port = htons(port);
}

std::string Ipv4Address::getIp()
{
    return mIp;
}

uint16_t Ipv4Address::getPort()
{
    return mPort;
}

struct sockaddr* Ipv4Address::getAddr()
{
    return (struct sockaddr*)&mAddr;
}
