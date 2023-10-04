#ifndef _BUFFER_H_
#define _BUFFER_H_
#include <stdlib.h>
#include <algorithm>
#include <stdint.h>
#include <assert.h>



// 类表示一个缓冲区，其中包含了一些用于操作缓冲区的方法和成员变量。

class Buffer
{
public:
    static const int initialSize;

    explicit Buffer() :
        mBufferSize(initialSize),
        mReadIndex(0),
        mWriteIndex(0)
    {
        mBuffer = (char*)malloc(mBufferSize);
    }

    ~Buffer()
    {
        free(mBuffer);
    }
    // 获取缓冲区中可读、可写和可插入数据的字节数。
    int readableBytes() const { return mWriteIndex - mReadIndex; }
    // 缓冲区中可写区域free区域的长度
    int writableBytes() const { return mBufferSize - mWriteIndex; }

    int prependableBytes() const { return mReadIndex; }
    // peek 窥视 返回可读数据的起始地址
    char* peek() { return begin() + mReadIndex; }

    // 查看缓冲区中的数据。
    const char* peek() const { return begin() + mReadIndex; }

    const char* findCRLF() const {
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findCRLF(const char* start) const {
        assert(peek() <= start);
        assert(start <= beginWrite());
        const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    const char* findLastCrlf() const {    
        const char* crlf = std::find_end(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    // 用于读取并移除缓冲区中的数据。
    void retrieve(int len) {
        assert(len <= readableBytes());
        if (len < readableBytes()) {
            mReadIndex += len;
        } else{
            // 所有数据都读了,把readerIndex_和writerIndex_复位
            retrieveAll();
        }
    }

    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    // 复位
    void retrieveAll() {
        mReadIndex = 0;
        mWriteIndex = 0;
    }
    // 用于获取可写位置和更新写位置。
    char* beginWrite() { return begin() + mWriteIndex; }

    const char* beginWrite() const { return begin() + mWriteIndex; }

    void hasWritten(int len)
    {
        assert(len <= writableBytes());
        mWriteIndex += len;
    }

    void unwrite(int len)
    {
        assert(len <= readableBytes());
        mWriteIndex -= len;
    }

    /* 确保有足够的空间 */
    void ensureWritableBytes(int len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }
    // 给缓冲区扩容 or 移位
    void makeSpace(int len) {
        // 长度不够,说明要扩容
        if (writableBytes() + prependableBytes() < len) //如果剩余空间不足
        {
            /* 扩大空间 */            
            mBufferSize = mWriteIndex+len;
            mBuffer = (char*)realloc(mBuffer, mBufferSize);
        } else {//剩余空间足够,移动内容
            int readable = readableBytes();
            std::copy(begin()+mReadIndex,
                    begin()+mWriteIndex,
                    begin());
            mReadIndex = 0;
            mWriteIndex = mReadIndex + readable;
            assert(readable == readableBytes());
        }
    }
    // 向缓冲区中添加数据。
    void append(const char* data, int len) {
        ensureWritableBytes(len); //调整空间
        std::copy(data, data+len, beginWrite()); //拷贝数据
        hasWritten(len); //重新调节写位置
    }

    void append(const void* data, int len){
        append((const char*)(data), len);
    }

    int read(int fd);
    int write(int fd);

private:
    char* begin() { return mBuffer; }

    const char* begin() const { return mBuffer; }

private:
    char* mBuffer; // 指向缓冲区的指针
    int mBufferSize; // 缓冲区的大小
    int mReadIndex; // 读位置
    int mWriteIndex; // 写位置

    static const char* kCRLF; // 表示回车换行符。
};

#endif //_BUFFER_H_