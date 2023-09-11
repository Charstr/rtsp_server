
// https://www.bilibili.com/video/BV1eb411F74G?p=7 8
// TCP通信的基本流程, 服务端，客户端
#include <cstdlib>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 23333
#define ERR_EXIT(m)\
        do{\
            perror(m);\
            exit(EXIT_FAILURE);\
        } while(0);

void doClient(int srvSendToClientSockfd){
    // 这里进行循环处理,客户端关闭，要跳出循环处理的过程
    char recvBuf[1024] ={0};
    while(true){
        memset(recvBuf, 0, sizeof(recvBuf));
        // read from srvSendToClientSockfd 
        int ret = read(srvSendToClientSockfd, recvBuf, sizeof(recvBuf));
        if (ret == 0){
            printf("client close\n");// 客户端关闭，要跳出循环处理的过程
            break;
        } else if(ret==-1) ERR_EXIT("read");
        // 服务端输出并发送给客户端
        fputs(recvBuf, stdout);
        write(srvSendToClientSockfd, recvBuf, ret);
    }
}
int main(int argc, char *argv[]){

    // 1. 创建套接字
    int srvListenSocketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(srvListenSocketfd<0){
        ERR_EXIT("socket");
    }

    // 2. 分配套接字地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    // htons 和 htonl是宏定义，主要为了避免大小端的问题
    serverAddr.sin_port = htons(SERVER_PORT);
    // INADDR_ANY表示绑定到任何可用的本地IP地址。
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int on = 1;
    // 确保time_wait状态下同一端口仍可使用
    if(setsockopt(srvListenSocketfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))<0){
        ERR_EXIT("setsockopt");
    }
    // 3. 绑定套接字地址
    if(bind(srvListenSocketfd, (const sockaddr*)&serverAddr, sizeof(serverAddr))<0){
        ERR_EXIT("bind");
    }
    // 4. 等待连接请求状态
    if(listen(srvListenSocketfd, SOMAXCONN)<0){
        ERR_EXIT("listen");
    }
    // 
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    socklen_t len = sizeof(clientAddr);

    pid_t pid;
    // 循环处理进程,每一个客户端进来，都会创建进程处理通信
    
    while(true){

        int srvSendToClientSockfd;
        // 服务端连接成功时候确定客户端的地址clientAddr
        // 服务端需要两个套接字，一个监听套接字，一个处理连接套接字。
        // srvListenSocketfd是监听套接字(主动套接字)用来发起与特定客户端的连接,处理三次握手数据，三次握手完成会放到已连接队列
        // srvSendToClientSockfd是处理连接套接字(被动套接字,已连接套接字)用来监听和接受客户端连接请求, 用于与特定客户端建立连接后的通信
        
        srvSendToClientSockfd = accept(srvListenSocketfd, (sockaddr*)&clientAddr, &len);

        printf("IP = %s port = %d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
        if(srvSendToClientSockfd<0){
            ERR_EXIT("accept");
        }

        pid = fork();
        if(pid==-1){
            ERR_EXIT("PID");
        }
        // 父子进程共享文件描述符
        if(pid==0){ // 子进程处理客户端的处理细节,不需要处理监听
            close(srvListenSocketfd);
            doClient(srvSendToClientSockfd);
            // 客户端关闭, 该进程没有存在的价值,exit, 否则子进程也会accept去进行连接
            exit(EXIT_SUCCESS);
        }else{ // 一个父进程处理accept,专门接收客户端连接
            close(srvSendToClientSockfd);
        }
    }
    close(srvListenSocketfd);
    return 0;
}