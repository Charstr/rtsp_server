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

// 工厂模式内存可能没有释放
int main(int argc, char *argv[]) {

	std::string videofile = "/home/lcp/myProj/rtsp_server/rtspExample/test1.264";
	std::string audiofile = "/home/lcp/myProj/rtsp_server/rtspExample/test.aac";

	// Logger::setLogFile("xxx.log");
	Logger::setLogLevel(Logger::LogWarning);

	std::shared_ptr<EventScheduler> scheduler =
		EventScheduler::createNew(EventScheduler::POLLER_EPOLL);

	std::shared_ptr<ThreadPool> threadPool = ThreadPool::createNew(2);

	// UsageEnvironment封装了EventScheduler和ThreadPool方便对调度和线程池的调用
	std::shared_ptr<UsageEnvironment> env =
		UsageEnvironment::createNew(scheduler.get(), threadPool.get());

	/*--------------server---------------------*/
	// 传递的是服务器的listen fd，栈上
	Ipv4Address ipAddr("0.0.0.0", 8554);

	// 管理RTSP连接和媒体会话,负责处理客户端连接
	std::shared_ptr<RtspServer> server = RtspServer::createNew(env.get(), ipAddr);

	/*--------------media---------------------*/

	std::shared_ptr<MediaSource> videoMediaSource =
		H264FileMediaSource::createNew(env.get(), videofile);

	// 基类的来动态调用
	std::shared_ptr<RtpSink> videoRtpSink =
		H264RtpSink::createNew(env.get(), videoMediaSource.get());

	std::shared_ptr<MediaSession> session = MediaSession::createNew("live");

	std::shared_ptr<MediaSource> audioSource = AACFileMeidaSource::createNew(env.get(), audiofile);
	std::shared_ptr<RtpSink> audioRtpSink = AACRtpSink::createNew(env.get(), audioSource.get());

	// 资源消费者
	session->addRtpSink(MediaSession::TrackId0, videoRtpSink.get());
	session->addRtpSink(MediaSession::TrackId1, audioRtpSink.get());

	// session->startMulticast(); //多播

	server->addMeidaSession(session.get());

	// 执行tcp连接中的listen操作，在监听socket上启动listen函数，同时将mAcceptIOEvent注册到调度器
	server->start();

	std::cout << "Play the media using the URL \"" << server->getUrl(session.get()) << "\""
			  << std::endl;

	// 程序完成了对socket的监听，调用EventScheduler::loop
	// 后程序开始循环监听socket的可读事件。 当新连接请求建立时，可读事件触发，此时该事件对应的
	// callback 在 EventScheduler::loop 中被调用。该事件的 callback 实际上就是
	// Acceptor::handleRead() 方法。调用accept函数返回实现了连接的建立，得到一个已连接用于通信的
	// connfd。然后将已连接connfd的事件mAcceptIOEvent注册到EventScheduler::loop中
	// 这样一个新连接已建立好且该连接的socket可读事件也加入到了EventScheduler::loop中

	scheduler->loop();

	return 0;
}