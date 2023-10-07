#include "Buffer.h"
#include "server/SocketsOps.h"

#include <unistd.h>

const int Buffer::initialSize = 1024;
const char* Buffer::kCRLF = "\r\n";

// 从fd上读取数据 LT模式
int Buffer::read(int fd) {

    // saved an ioctl()/FIONREAD call to tell how much to read
    char extrabuf[65536]; // 栈上开辟的内存空间 64K
    struct iovec vec[2];
    const int writable = writableBytes();// 可写缓冲区的大小
    vec[0].iov_base = begin()+mWriteIndex;// 第一块缓冲区起始地址
    vec[0].iov_len = writable;// 当我们用readv从socket缓冲区读数据,首先会填充这个vec[0],也就是我们的buffer
    vec[1].iov_base = extrabuf;// 第二块缓冲区,如果buffer填满了,就会填到这里
    vec[1].iov_len = sizeof(extrabuf);// 栈空间大小
    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const int n = sockets::readv(fd, vec, iovcnt);
    if (n < 0){
        return -1; // 出错了
    }else if (n <= writable){
        mWriteIndex += n;// 说明buffer空间够用
    } else {
        // buffer空间不够,extrabuf上也有数据,用append把这部分数据拷贝过来
        mWriteIndex = mBufferSize; 
        append(extrabuf, n - writable);
    }
    return n;
}

int Buffer::write(int fd) {
    
    return sockets::write(fd, peek(), readableBytes());
}