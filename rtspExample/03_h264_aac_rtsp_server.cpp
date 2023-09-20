#include <iostream>

#include "base/Logging.h"
#include "net/UsageEnvironment.h"
#include "base/ThreadPool.h"
#include "net/EventScheduler.h"
#include "net/Event.h"
#include "net/RtspServer.h"
#include "net/MediaSession.h"
#include "net/InetAddress.h"
#include "net/H264FileMediaSource.h"
#include "net/H264RtpSink.h"
#include "net/AACFileMediaSource.h"
#include "net/AACRtpSink.h"

int main(int argc, char* argv[])
{
    if(argc !=  3)
    {
        std::cout<<"Usage: "<<argv[0]<<" <h264 file> <aac file>"<<std::endl;
        return -1;
    }

    //Logger::setLogFile("xxx.log");
    Logger::setLogLevel(Logger::LogWarning);

    // rtspserver所有执行都是回调，回调来源通过scheduler进行执行
    EventScheduler* scheduler = EventScheduler::createNew(EventScheduler::POLLER_SELECT);
    ThreadPool* threadPool = ThreadPool::createNew(2);
    UsageEnvironment* env = UsageEnvironment::createNew(scheduler, threadPool);

    Ipv4Address ipAddr("0.0.0.0", 8554);
    // 
    RtspServer* server = RtspServer::createNew(env, ipAddr);
    MediaSession* session = MediaSession::createNew("live");
    MediaSource* videoSource = H264FileMediaSource::createNew(env, argv[1]);
    RtpSink* videoRtpSink = H264RtpSink::createNew(env, videoSource);
    MediaSource* audioSource = AACFileMeidaSource::createNew(env, argv[2]);
    RtpSink* audioRtpSink = AACRtpSink::createNew(env, audioSource);

    session->addRtpSink(MediaSession::TrackId0, videoRtpSink);
    session->addRtpSink(MediaSession::TrackId1, audioRtpSink);
    //session->startMulticast(); //多播
    // 读取资源传给sink并加到track中，然后把session加到session 管理器
    server->addMeidaSession(session);
    server->start();

    std::cout<<"Play the media using the URL \""<<server->getUrl(session)<<"\""<<std::endl;

    env->scheduler()->loop();

    return 0;
}