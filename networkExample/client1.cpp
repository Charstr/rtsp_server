// https://www.bilibili.com/video/BV1eb411F74G?p=7 8
// TCP通信的基本流程, 客户端
#include <cstddef>
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
        } while(0);\


int main(int argc, char *argv[]){

    int clientSocketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(clientSocketfd<0){
        ERR_EXIT("socket");
    }

    //初始化要连接的对方的地址 
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    //客户端调用 connect()函数将主动套接字 clientSocketfd 与远程服务器进行连接，
    if(connect(clientSocketfd, (const sockaddr*)&serverAddr, sizeof(serverAddr))<0){
        ERR_EXIT("connect");
    }

    char sendBuf[1024], recvBuf[1024];
    while(fgets(sendBuf, sizeof(sendBuf), stdin)!=nullptr){
        // 通过本机的socketfd写
        write(clientSocketfd, sendBuf, sizeof(sendBuf));
        // 通过本机的socketfd写
        int ret = read(clientSocketfd, recvBuf, sizeof(recvBuf));
        if (ret == 0){
            printf("server close\n");
            break;
        } else ERR_EXIT("read");
        fputs(recvBuf, stdout);
        memset(recvBuf, 0, sizeof recvBuf);
        memset(sendBuf, 0, sizeof sendBuf);
    }
    close(clientSocketfd);
    return 0;
}