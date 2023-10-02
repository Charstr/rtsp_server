#include <cstddef>
#include <iostream>

#include "base/Logging.h"

#include "schedule/UsageEnvironment.h"
#include "schedule/threadPool/ThreadPool.h"
#include "schedule/EventScheduler.h"
#include "schedule/Event.h"

#include "media/MediaSession.h"
#include "media/H264FileMediaSource.h"
#include "media/H264RtpSink.h"

#include "server/RtspServer.h"
#include "server/InetAddress.h"

// 基于Reactor的事件处理、非阻塞IO的应用，以及线程池的使用。
int main(int argc, char* argv[])
{
    // if(argc !=  2)
    // {
    //     std::cout<<"Usage: "<<argv[0]<<" <h264 file>"<<std::endl;
    //     return -1;
    // }

    // std::string fileanme = "/home/more/proj/rtsp_server/rtspExample/test.h264";
    std::string fileanme = "./test.h264";

    //Logger::setLogFile("xxx.log");
    Logger::setLogLevel(Logger::LogWarning);

    /*

    实现步骤和详细流程：
    1. Mutex和Condition用于多线程同步的类。Mutex用于创建互斥锁，Condition用于线程间的条件变量通信。
    2. threadPool类用于创建和管理线程池，其中包括线程的创建和管理，以及添加任务到任务队列等功能。
    3. EventScheduler类用于事件调度，其中包括定时器和事件循环的实现。
    4. UsageEnvironment类用于封装与事件调度器和线程池相关的环境信息,构造函数接受EventScheduler*和ThreadPool*作为参数，并将它们保存在成员变量中。将事件调度器和线程池与其他部分隔离开来，并提供访问它们的接口。
    5. TimerManager类用于管理定时器任务,负责跟踪定时器的触发时间和相应的回调函数,在某个时间点或以固定时间间隔触发回调函数,核心功能包括创建、管理和取消定时器任务，以及确保精确的时间控制。与事件调度器集成，以在事件循环中触发定时器。TimerManager,在上下文中，它可能使用操作系统提供的高分辨率时钟来确保定时器的准确触发。

    6. Poller类用于执行事件轮询，监测I/O事件（如套接字可读、可写）的发生，并通知事件调度器。与事件调度器协作，确保及时地通知事件调度器关于I/O事件的发生。
    */

    // 创建任务调度器
    EventScheduler* scheduler = EventScheduler::createNew(EventScheduler::POLLER_EPOLL);

    // 创建一个线程池对象的实例,里边通过一个vector存储所有的线程
    // 读取文件在子线程完成
    ThreadPool* threadPool = ThreadPool::createNew(2);

    // UsageEnvironment封装了EventScheduler和ThreadPool方便对调度和线程池的调用
    UsageEnvironment* env = UsageEnvironment::createNew(scheduler, threadPool);
    // 上边通过申请内存并进行对象构造之后，在函数结束之后自动调用析构函数，中止对应的线程并调用析构释放内存

    /*--------------server---------------------*/
    // 传递的是服务器的listen fd
    Ipv4Address ipAddr("0.0.0.0", 8554);


    // 是一个RTSP服务器的类，用于管理RTSP连接和媒体会话,负责处理客户端连接
    RtspServer* server = RtspServer::createNew(env, ipAddr);

    /*--------------media---------------------*/
    MediaSource* mediaSource = H264FileMediaSource::createNew(env, fileanme);

    //MediaSource* mediaSource = H264FileMediaSource::createNew(env, argv[1]);
    
    // rtpSink资源消费者,资源生产者是mediaSource
    RtpSink* rtpSink = H264RtpSink::createNew(env, mediaSource);
    
    MediaSession* session = MediaSession::createNew("live");

    session->addRtpSink(MediaSession::TrackId0, rtpSink);
    //session->startMulticast(); //多播

    /* 向服务器添加会话 */
    server->addMeidaSession(session);
    // 开始listen,将接受连接的IO事件mAcceptIOEvent添加到事件调度器的循环中
    // mEnv->scheduler()->addIOEvent(mAcceptIOEvent);
    server->start();

    std::cout<<"Play the media using the URL \""<<server->getUrl(session)<<"\""<<std::endl;
    /* 循环处理事件 */
    env->scheduler()->loop();

    return 0;
}