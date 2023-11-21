#include <cstddef>
#include <iostream>

#include "base/Logging.h"
#include "media/AACFileMediaSource.h"
#include "media/AACRtpSink.h"
#include "media/H264FileMediaSource.h"
#include "media/H264RtpSink.h"
#include "media/MediaSession.h"
#include "schedule/Event.h"
#include "schedule/EventScheduler.h"
#include "schedule/UsageEnvironment.h"
#include "schedule/threadPool/ThreadPool.h"
#include "server/InetAddress.h"
#include "server/RtspServer.h"

int main(int argc, char *argv[]) {

	std::string videofile = "/home/lcp/myProj/rtsp_server/rtspExample/test1.264";
	std::string audiofile = "/home/lcp/myProj/rtsp_server/rtspExample/test.aac";

	Logger::setLogFile("xxx.log");
	Logger::setLogLevel(Logger::LogWarning);

	std::shared_ptr<EventScheduler> scheduler =
		EventScheduler::createNew(EventScheduler::POLLER_EPOLL);

	// 创建2个线程的线程池
	ThreadPool *threadPool = ThreadPool::createNew(4);

	UsageEnvironment *env = UsageEnvironment::createNew(scheduler.get(), threadPool);

	/*--------------server---------------------*/
	// 传递的是服务器的listen fd
	Ipv4Address ipAddr("0.0.0.0", 8554);

	std::shared_ptr<RtspServer> server = RtspServer::createNew(env, ipAddr);

	/*--------------media---------------------*/
	// MediaSource创建初始化一个缓冲区，对应到mAVFrameInputQueue队列，设置线程的任务回调函数MediaSource::taskCallback，通过多态读取调用H264FileMediaSource::readFrame从h264文件读取一个AVFrame到临时缓冲到mAVFrameInputQueue，指定位置是设置的缓冲区mAVFrames，然后取出到mAVFrameOutputQueue
	// 添加DEFAULT_FRAME_NUM个线程任务到线程任务队列mTaskQueue

	MediaSource *videoMediaSource = H264FileMediaSource::createNew(env, videofile);

	RtpSink *videoRtpSink = H264RtpSink::createNew(env, videoMediaSource);
	MediaSession *session = MediaSession::createNew("live");

	MediaSource *audioSource = AACFileMeidaSource::createNew(env, audiofile);
	RtpSink *audioRtpSink = AACRtpSink::createNew(env, audioSource);

	session->addRtpSink(MediaSession::TrackId0, videoRtpSink);
	session->addRtpSink(MediaSession::TrackId1, audioRtpSink);

	/*
	1. 设置session
	track0的消费者指针videoRtpSink，并设置videoRtpSink发送某个track的rtp数据包使用的回调函数.
	2. 回调是在具体的H264RtpSink的handleFrame中进行调用, handleFrame把一个AVFrame分成多个rtp包发送
	3. 回调函数会遍历某个track的mRtpInstances链表，调用RtpInstance类send函数发送各个实例的rtp数据包.
	*/
	session->addRtpSink(MediaSession::TrackId0, videoRtpSink);
	session->addRtpSink(MediaSession::TrackId1, audioRtpSink);
	// session->startMulticast(); //多播

	// 添加mSessionName和MediaSession指针的映射
	server->addMeidaSession(session);

	// 执行tcp连接中的listen操作，在监听socket上启动listen函数，同时将mAcceptIOEvent注册到调度器
	server->start();

	std::cout << "Play the media using the URL \"" << server->getUrl(session) << "\"" << std::endl;

	// 到这里，程序完成了对socket的监听，调用EventScheduler::loop
	// 后程序开始循环监听socket的可读事件。 当新连接请求建立时，可读事件触发，此时该事件对应的
	// callback 在 EventScheduler::loop 中被调用。该事件的 callback 实际上就是
	// Acceptor::handleRead() 方法。调用accept函数返回实现了连接的建立，得到一个已连接用于通信的
	// connfd。然后将已连接connfd的事件mAcceptIOEvent注册到EventScheduler::loop中
	// 这样一个新连接已建立好且该连接的socket可读事件也加入到了EventScheduler::loop中

	scheduler->loop();
	return 0;
}