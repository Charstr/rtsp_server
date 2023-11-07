#ifndef _RTSPSERVER_H_
#define _RTSPSERVER_H_
#include <map>
#include <string>
#include <vector>

#include "RtspConnection.h"
#include "TcpServer.h"
#include "media/MediaSession.h"
#include "schedule/Event.h"
#include "schedule/UsageEnvironment.h"

#include "schedule/threadPool/Mutex.h"

class RtspConnection;

// 用于处理RTSP服务器的逻辑。其中包括添加媒体会话、查找媒体会话、处理新连接、处理断开连接等功能。
class RtspServer : public TcpServer {
public:
	// 创建一个新的RtspServer实例
	static RtspServer *createNew(UsageEnvironment *env, Ipv4Address &addr);

	RtspServer(UsageEnvironment *env, const Ipv4Address &addr);
	virtual ~RtspServer();
	// 获取UsageEnvironment实例
	UsageEnvironment *envir() const { return mEnv; }
	// 向RtspServer添加媒体会话
	bool addMeidaSession(MediaSession *mediaSession);
	// 根据名称查找媒体会话
	MediaSession *loopupMediaSession(std::string name);
	// 获取媒体会话的URL
	std::string getUrl(MediaSession *session);

protected:
	// 处理新连接的回调函数
	virtual void handleNewConnection(int connfd);
	// 处理断开连接的回调函数
	static void disconnectionCallback(void *arg, int sockfd);

	void handleDisconnection(int sockfd);
	// 触发事件回调,处理断开连接的
	static void triggerCallback(void *);
	// 处理断开连接列表
	void handleDisconnectionList();

private:
	std::map<std::string, MediaSession *> mMediaSessions; // 存储媒体会话的容器
	std::map<int, RtspConnection *> mConnections;		  // 存储RtspConnection的容器
	std::vector<int> mDisconnectionlist;				  // 存储断开连接的描述符的队列
	TriggerEvent *mTriggerEvent;						  // 触发事件
	Mutex *mMutex;										  // 互斥锁，用于线程安全
};

#endif //_RTSPSERVER_H_