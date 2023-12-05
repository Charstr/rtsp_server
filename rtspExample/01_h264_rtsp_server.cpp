#include <cstddef>
#include <cstdio>
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

	std::string videofile = "/home/lcp/myProj/rtsp_server/rtspExample/test.h264";
	std::string audiofile = "/home/lcp/myProj/rtsp_server/rtspExample/output.aac";

	Logger::setLogFile("xxx.log");
	Logger::setLogLevel(Logger::LogDebug);

	std::shared_ptr<EventScheduler> scheduler =
		EventScheduler::createNew(EventScheduler::POLLER_EPOLL);

	std::shared_ptr<ThreadPool> threadPool = ThreadPool::createNew(2);

	std::shared_ptr<UsageEnvironment> env =
		UsageEnvironment::createNew(scheduler.get(), threadPool.get());

	/*--------------server---------------------*/

	Ipv4Address ipAddr("0.0.0.0", 8554);

	std::shared_ptr<RtspServer> server = RtspServer::createNew(env.get(), ipAddr);

	/*--------------media---------------------*/

	std::shared_ptr<MediaSource> videoMediaSource =
		H264FileMediaSource::createNew(env.get(), videofile);
	std::shared_ptr<RtpSink> videoRtpSink =
		H264RtpSink::createNew(env.get(), videoMediaSource.get());

	std::shared_ptr<MediaSource> audioSource = AACFileMeidaSource::createNew(env.get(), audiofile);
	std::shared_ptr<RtpSink> audioRtpSink = AACRtpSink::createNew(env.get(), audioSource.get());

	std::shared_ptr<MediaSession> session = MediaSession::createNew("live");
	session->addRtpSink(MediaSession::TrackId0, videoRtpSink.get());
	session->addRtpSink(MediaSession::TrackId1, audioRtpSink.get());

	// session->startMulticast(); //多播

	server->addMeidaSession(session.get());
	server->start();

	printf("Play the media using the URL \"%s\"\n", server->getUrl(session.get()).c_str());

	scheduler->loop();

	return 0;
}