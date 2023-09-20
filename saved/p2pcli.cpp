// https://www.bilibili.com/video/BV1eb411F74G?p=7 8
// TCP通信的基本流程, 客户端
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define SERVER_PORT 23333
#define ERR_EXIT(m)\
        do{\
            perror(m);\
            exit(EXIT_FAILURE);\
        } while(0);\

void handler(int sig){
    printf("recv sig = %d\n", sig);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]){
    // 1. 创建套接字
    int cliSocketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(cliSocketfd<0){
        ERR_EXIT("socket");
    }

    // 2. 要连接的对方分配套接字地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    //客户端调用 connect()函数将主动套接字 cliSocketfd 与远程服务器进行连接，
    if(connect(cliSocketfd, (const sockaddr*)&serverAddr, sizeof(serverAddr))<0){
        ERR_EXIT("connect");
    }

    char recvbuf[1024];
    char sendbuf[1024];
    pid_t pid;
    pid = fork();
    if (pid == -1) ERR_EXIT("fork");
    if (pid == 0) { // child
        
        while (1){
            int ret = read(cliSocketfd, recvbuf, sizeof(recvbuf));    // 服务器读取
            if(ret == -1) {ERR_EXIT("read");}
            else if(ret == 0){
                printf("peer close\n");
                break;
            }
            fputs(recvbuf, stdout); // 服务器返回数据输出
        }
        // 进程结束应该关闭socket
        printf("child close\n");
        //exit(EXIT_SUCCESS);
        kill(pid, SIGUSR1);
        close(cliSocketfd);
    } else{ // parents
          // 键盘输入获取
        //signal(SIGUSR1, handler);
        while (fgets(sendbuf, sizeof(sendbuf), stdin) != nullptr) {
            write(cliSocketfd, sendbuf, sizeof sendbuf); // 写入服务器
            // 清空
            memset(sendbuf, 0, sizeof sendbuf);
        }
        // 父进程退出的时候子进程还没有退出,可通过信号实现
        printf("parent close\n");
        exit(EXIT_SUCCESS);
        close(cliSocketfd);
    }
    // 5. 断开连接
    close(cliSocketfd);

    return 0;
}