#ifndef _H264FILE_MEDIA_SOURCE_H_
#define _H264FILE_MEDIA_SOURCE_H_
#include <string>

#include "MediaSource.h"
#include "schedule/UsageEnvironment.h"
#include "schedule/threadPool/ThreadPool.h"

// source 生产者从本地读取文件模拟资源
// 生产者和消费者都来自于一个顶级父类,生产者父类是MediaSource
class H264FileMediaSource : public MediaSource {
public:
	static H264FileMediaSource *createNew(UsageEnvironment *env, std::string file);

	H264FileMediaSource(UsageEnvironment *env, const std::string &file);
	~H264FileMediaSource();

protected:
	virtual void readFrame();

private:
	// 从H264文件中获取帧数据。
	int getFrameFromH264File(int fd, uint8_t *frame, int size);

private:
	std::string mFile;
	int mFd; // 于存储文件描述符
};

#endif //_H264FILE_MEDIA_SOURCE_H_