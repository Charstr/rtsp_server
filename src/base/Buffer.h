#ifndef _BUFFER_H_
#define _BUFFER_H_
#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

/*
+-------------------------+----------------------+---------------------+
|    prependable bytes    |    readable bytes    |    writable bytes   |
|                         |      (CONTENT)       |                     |
+-------------------------+----------------------+---------------------+
|                         |                      |                     |
0        <=           readerIndex     <=     writerIndex             size


prependable记录数据的长度，写数据的时候偏移writerIndex，读数据时候偏移readerIndex。
要写的数据超出了可写大小 writable bytes，就把readerIndex这个下标开始的数据往前边挪
继续往里编写，如果还是不够写就要进行扩容。
*/

class Buffer {
public:
	// 类内声明，类外初始化
	static const int initialSize;

	explicit Buffer() : mBufferSize(initialSize), mReadIndex(0), mWriteIndex(0) {
		mBuffer = (char *)malloc(mBufferSize);
	}

	~Buffer() {
		free(mBuffer);
	}

	// 可读数据长度
	int readableBytes() const {
		return mWriteIndex - mReadIndex;
	}
	// 可写长度
	int writableBytes() const {
		return mBufferSize - mWriteIndex;
	}

	// 从begin开始的第几个开始读
	int prependableBytes() const {
		return mReadIndex;
	}

	// peek 返回可读数据的起始地址
	char *peek() {
		return begin() + mReadIndex;
	}

	// 查看缓冲区中的数据。
	const char *peek() const {
		return begin() + mReadIndex;
	}

	// 每次进来写偏移mWriteIndex，读之后才偏移mReadIndex
	// mWriteIndex应该是>=mReadIndex
	// begin()到begin()+mReadIndex这部分是已经读取过的，
	// begin() + mReadIndex到begin() + mWriteIndex是目前可读的
	// begin() + mWriteIndex到begin()+mBufferSize是目前可写的
	const char *findCRLF() const {
		// 从peek到beginWrite第一次出现kCRLF的位置，第一个字符的指针，找不到就返回beginWrite()
		const char *crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	const char *findCRLF(const char *start) const {
		assert(peek() <= start && start <= beginWrite());
		// assert(start <= beginWrite());
		const char *crlf = std::search(start, beginWrite(), kCRLF, kCRLF + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	// 找最后一个出现的\r\n
	const char *findLastCrlf() const {
		const char *crlf = std::find_end(peek(), beginWrite(), kCRLF, kCRLF + 2);
		return crlf == beginWrite() ? NULL : crlf;
	}

	// 需要进行复位操作
	void retrieve(int len) {
		assert(len <= readableBytes());
		// 应用只读取可读缓冲区数据的一部分(读取了len的长度)
		if (len < readableBytes()) {
			mReadIndex += len; // 移动可读缓冲区指针
		} else {
			// 所有消息都读完了len == readableBytes()就需要把mReadIndex和mWriteIndex复位
			retrieveAll();
		}
	}

	// 解析method时候，进来的end是\r\n位置后边的下标
	void retrieveUntil(const char *end) {
		assert(peek() <= end && end <= beginWrite());
		// assert(end <= beginWrite());
		retrieve(end - peek());
	}

	// 全部读完，则直接将可读缓冲区指针移动到写缓冲区指针那
	void retrieveAll() {
		mReadIndex = 0;
		mWriteIndex = 0;
	}
	// 用于获取可写位置和更新写位置。
	char *beginWrite() {
		return begin() + mWriteIndex;
	}

	const char *beginWrite() const {
		return begin() + mWriteIndex;
	}

	void hasWritten(int len) {
		assert(len <= writableBytes());
		mWriteIndex += len;
	}

	void unwrite(int len) {
		assert(len <= readableBytes());
		mWriteIndex -= len;
	}

	/* 确保有足够的空间 */
	void ensureWritableBytes(int len) {
		if (writableBytes() < len) {
			// 扩容函数
			makeSpace(len);
		}
		assert(writableBytes() >= len);
	}

	// 给缓冲区扩容 or 移位
	void makeSpace(int len) {
		/*
		mBuffer是内存的起始地址
		mBuffer
		+-------------------------+----------------------+---------------------+
		|    prependable bytes    |    readable bytes    |    writable bytes   |
		|                         |      (CONTENT)       |                     |
		+-------------------------+----------------------+---------------------+
		|                         |                      |                     |
		0       	<=        readerIndex     <=     writerIndex              size

		0--readerIndex这部分是已经读取过的内容，可重写覆盖，readerIndex--writerIndex
		这部分是可以读的内容，但是没有读，所以不能覆盖掉，writerIndex--size部分是可写的

		所以先判断这两部分如果比要写的len少，那就给mBuffer重新扩容至mBufferSize = mWriteIndex + len;

		如果是比len多，那就是可以写的下，不需要扩容
		要把readable = readerIndex--writerIndex这一部分可以读取的移动到mBuffer开始的位置
		也就是begin()+readerIndex到 begin()+writerIndex这一部分移动到缓冲区前边
		然后readerIndex=0，writerIndex=readable，
		移动之后可读的这段长度应该是不变的

		*/
		if (writableBytes() + prependableBytes() < len) {
			/* 扩大空间 */
			mBufferSize = mWriteIndex + len;
			mBuffer = (char *)realloc(mBuffer, mBufferSize);
		} else {
			// 整个buffer够用，将后面移动到前面继续分配
			int readable = readableBytes();
			std::copy(begin() + mReadIndex, begin() + mWriteIndex, begin());
			mReadIndex = 0;
			mWriteIndex = mReadIndex + readable;
			assert(readable == readableBytes());
		}
	}

	void append(const char *data, int len) {
		ensureWritableBytes(len); // 调整空间
		// 把[data, data+len]内存上的数据添加到缓冲区中
		std::copy(data, data + len, beginWrite());
		hasWritten(len); // 重新调节写位置
	}

	void append(const void *data, int len) {
		append((const char *)(data), len);
	}
	// 从fd上读取数据
	int read(int fd);
	// 通过fd发送数据
	int write(int fd);

private:
	// 获取buffer_起始地址
	char *begin() {
		return mBuffer;
	}

	const char *begin() const {
		return mBuffer;
	}

private:
	// 这里也可以改成vector实现
	char *mBuffer; // 指向实际的数据缓冲区
	int mBufferSize; // 缓冲区的大小
	int mReadIndex; // 读位置
	int mWriteIndex; // 写位置

	static const char *kCRLF; // 表示回车换行符。
};

#endif //_BUFFER_H_