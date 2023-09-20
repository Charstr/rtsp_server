
// https://www.bilibili.com/video/BV1eb411F74G?p=7 8
// TCP通信的基本流程, p2p chat
#include <csignal>
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

void handler(int sig){
    printf("recv sig = %d\n", sig);
    exit(EXIT_SUCCESS);
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
    serverAddr.sin_port = htons(SERVER_PORT);
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

    // 服务端连接成功时候确定客户端的地址clientAddr
    // 服务端需要两个套接字，一个监听套接字，一个处理连接套接字。
    // srvListenSocketfd是监听套接字(主动套接字)用来发起与特定客户端的连接,处理三次握手数据，三次握手完成会放到已连接队列
    // srvSendToClientSockfd是处理连接套接字(被动套接字,已连接套接字)用来监听和接受客户端连接请求,不能去接受连接

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    socklen_t len = sizeof(clientAddr);
    int srvSendToClientSockfd = accept(srvListenSocketfd, (sockaddr*)&clientAddr, &len);
    if(srvSendToClientSockfd<0){
        ERR_EXIT("accept");
    }

    printf("IP = %s port = %d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

    // 点对点不考虑一个服务端跟多个客户端通信
    // 服务端在接受数据的同时还能发送数据，就需要两个进行，一个负责接收数据一个负责发送数据
    // fork()创建子进程时会返回两次。一次在父进程中执行，一次在子进程中执行。
    // 6. 数据交换
    pid_t pid;
    pid = fork();

    if (pid == -1)
    {
        ERR_EXIT("fork");
    }
    if (pid == 0) {   // 子进程
        signal(SIGUSR1, handler); // 使用信号通知父进程关闭的同时关闭子进程
        char sendbuf[1024];
        while (fgets(sendbuf, sizeof(sendbuf), stdin) != nullptr) {
            write(srvSendToClientSockfd, sendbuf, sizeof(sendbuf));
            memset(sendbuf, 0, sizeof(sendbuf));
        }
        printf("child close\n");
        close(srvSendToClientSockfd);
        close(srvListenSocketfd);
        
        exit(EXIT_SUCCESS);
    } else{
        char recvbuf[1024];
        while (1){
            memset(recvbuf, 0, sizeof recvbuf);
            int ret = read(srvSendToClientSockfd, recvbuf, sizeof recvbuf);
            if(ret == -1) {ERR_EXIT("read");}
            else if(ret == 0){
                printf("peer close\n");
                break;
            }
            fputs(recvbuf, stdout);
        }
        printf("parent close\n");
        close(srvSendToClientSockfd);
        close(srvListenSocketfd);
        kill(pid,SIGUSR1);
        exit(EXIT_SUCCESS);
    }

    // 7. 断开连接
    close(srvSendToClientSockfd);
    close(srvListenSocketfd);
    return 0;
}