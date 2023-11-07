#include <cstddef>
#include <iostream>

#include "base/Logging.h"

#include "schedule/Event.h"
#include "schedule/EventScheduler.h"
#include "schedule/UsageEnvironment.h"
#include "schedule/threadPool/ThreadPool.h"

#include "media/H264FileMediaSource.h"
#include "media/H264RtpSink.h"
#include "media/MediaSession.h"

#include "server/InetAddress.h"
#include "server/RtspServer.h"

/*
基于Reactor模式，指有一个循环的过程，不断监听对应事件是否触发，事件触发时调用对应的 callback
进行处理。

事件包括Socket
可读写事件、定时器事件、rtsp读事件等。EventScheduler负责事件循环；Poller负责监听事件是否触发的部分；
acceptor 负责 accept 新连接，并将新连接分发到 subReactor。
*/

int main(int argc, char *argv[]) {

	std::string fileanme = "/home/lcp/envPkg/rtsp_server/rtspExample/test1.264";

	// Logger::setLogFile("xxx.log");
	Logger::setLogLevel(Logger::LogWarning);

	/*
	创建任务调度器。一些事件的添加：
	1. EventScheduler::EventScheduler
	mPoller->addIOEvent(mWakeIOEvent);唤醒事件

	2. TimerManager::TimerManager
	mPoller->addIOEvent(mTimerIOEvent); 定时事件，

	3. Acceptor::listen()：
	mEnv->scheduler()->addIOEvent(mAcceptIOEvent); 接受新连接的事件

	4. RtspServer::handleDisconnection
	mEnv->scheduler()->addTriggerEvent(mTriggerEvent); 断开连接

	5. TcpConnection::TcpConnection
	mEnv->scheduler()->addIOEvent(mTcpConnIOEvent); 发送数据
	*/

	EventScheduler *scheduler = EventScheduler::createNew(EventScheduler::POLLER_EPOLL);

	// 创建2个线程的线程池
	ThreadPool *threadPool = ThreadPool::createNew(2);

	// UsageEnvironment封装了EventScheduler和ThreadPool方便对调度和线程池的调用
	UsageEnvironment *env = UsageEnvironment::createNew(scheduler, threadPool);

	/*--------------server---------------------*/
	// 传递的是服务器的listen fd
	Ipv4Address ipAddr("0.0.0.0", 8554);

	// 管理RTSP连接和媒体会话,负责处理客户端连接
	RtspServer *server = RtspServer::createNew(env, ipAddr);

	/*--------------media---------------------*/
	// MediaSource创建初始化一个缓冲区，对应到mAVFrameInputQueue队列，设置线程的任务回调函数MediaSource::taskCallback，通过多态读取调用H264FileMediaSource::readFrame从h264文件读取一个AVFrame到临时缓冲到mAVFrameInputQueue，指定位置是设置的缓冲区mAVFrames，然后取出到mAVFrameOutputQueue
	// 添加DEFAULT_FRAME_NUM个线程任务到线程任务队列mTaskQueue
	MediaSource *videoMediaSource = H264FileMediaSource::createNew(env, fileanme);

	// h264消费者，把一个AVFrame分成多个rtp包发送，设置了fps然后定时启动
	// 一个session对应的一个source和sink
	RtpSink *videoRtpSink = H264RtpSink::createNew(env, videoMediaSource);

	// 传进去的字符串是mSessionName
	MediaSession *session = MediaSession::createNew("live");

	/*
	1. 设置session
	track0的消费者指针videoRtpSink，并设置videoRtpSink发送某个track的rtp数据包使用的回调函数.
	2. 回调是在具体的H264RtpSink的handleFrame中进行调用, handleFrame把一个AVFrame分成多个rtp包发送
	3. 回调函数会遍历某个track的mRtpInstances链表，调用RtpInstance类send函数发送各个实例的rtp数据包.
	*/
	session->addRtpSink(MediaSession::TrackId0, videoRtpSink);
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

	env->scheduler()->loop();

	return 0;
}