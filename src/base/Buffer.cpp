#include "Buffer.h"
#include "server/SocketsOps.h"

#include <unistd.h>

const int Buffer::initialSize = 1024;
const char *Buffer::kCRLF = "\r\n"; // 是两个字符

// 采用非阻塞 I/O 的 LT 模式（Level-Triggered 水平触发模式），
// 从accept返回的文件描述符connfd上读取数据。使用 readv
// 系统调用进行异步IO，支持一次性读取数据到多个缓冲区中。 没有处理 EAGAIN
// 的逻辑，意味着当connfd就绪，应用程序会持续尝试读取数据，直到读取返回
// EAGAIN（表示暂时没有数据可读）为止。

int Buffer::read(int fd) {

	// 栈上开辟的额外缓冲区 64K字节，用于当缓冲区空间不足时存储多余的数据。
	char extrabuf[65536];

	// iovec指定读取数据的缓冲区，结构体就是一个数据起始的指针和数据的长度
	struct iovec vec[2];
	// mBufferSize - mWriteIndex也就是mBuffer缓冲区还可以再写的字节数。
	const int writable = writableBytes();

	// begin()就是mBuffer开始的指针
	vec[0].iov_base = begin() + mWriteIndex; // mBuffer缓冲区起始地址
	vec[0].iov_len = writable;				 // mBuffer缓冲区还可以再写的字节数

	// mBuffer缓冲区写不下，就用第二块临时的缓冲区来写
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof(extrabuf); // 额外栈空间大小

	// 基于经验和应用场景考虑的
	// 如果当前缓冲区足够大，是有可能容纳即将读取的数据，那么只需使用一个 iovec
	// 结构体，将数据直接读入当前缓冲区即可。在这种情况下，iovcnt 被设置为 1。
	// 如果当前缓冲区的剩余空间不足以容纳即将读取的数据，就需要使用两个 iovec
	// 结构体。一个用于当前缓冲区，另一个用于额外的临时缓冲区
	// extrabuf。读取的数据会被分散存储在两个缓冲区中，确保不会丢失任何数据。

	const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
	// 从connfd读取数据，先直接保存到mBuffer，不够的再保存到extrabuf
	const int n = sockets::readv(fd, vec, iovcnt);
	if (n < 0) {
		return -1; // 出错了
	} else if (n <= writable) {
		mWriteIndex += n; // 说明buffer空间够用
	} else {
		// buffer空间不够,extrabuf上也有数据,用append把这部分数据拷贝过来
		// append不够会进行扩容
		mWriteIndex = mBufferSize;
		append(extrabuf, n - writable);
	}
	return n;
}

// 发送数据给客户端
int Buffer::write(int fd) { return sockets::write(fd, peek(), readableBytes()); }