#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "RtspConnection.h"
#include "RtspServer.h"
#include "base/Logging.h"
#include "base/New.h"
#include "media/MediaSession.h"

// 过解析请求消息的不同字段（方法、URL、CSeq等），它能够执行不同的操作，如发送OPTIONS响应、解析SDP描述、建立RTP连接等
static void getPeerIp(int sockfd, std::string &ip) {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	getpeername(sockfd, (struct sockaddr *)&addr, &addrlen);
	ip = inet_ntoa(addr.sin_addr);
}

RtspConnection *RtspConnection::createNew(RtspServer *rtspServer, int sockfd) {
	return New<RtspConnection>::allocate(rtspServer, sockfd);
}

RtspConnection::RtspConnection(RtspServer *rtspServer, int sockfd)
	: TcpConnection(rtspServer->envir(), sockfd), // sockf就是connfd
	  mRtspServer(rtspServer), mMethod(NONE), mTrackId(MediaSession::TrackIdNone),
	  mSessionId(rand()), mIsRtpOverTcp(true) { // udp/tcp传输，默认udp传输

	/*
	1.
	TcpConnection根据Acceptor类accpet接受连接后返回的connfd，创建实际建立连接之后的用于传输的事件mTcpConnIOEvent，并设置处理读写、异常的回调函数，rtsp
	server默认只启用读事件，然后把事件mTcpConnIOEvent加入到事件调度中
	*/

	// 初始化rtp和rtsp示例，这个在setup的时候才有
	for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
		mRtpInstances[i] = NULL;
		mRtcpInstances[i] = NULL;
	}
	// 获取客户端的IP
	getPeerIp(sockfd, mPeerIp);
}

RtspConnection::~RtspConnection() {

	for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
		if (mRtpInstances[i]) {
			if (mSession)
				mSession->removeRtpInstance(mRtpInstances[i]);
			Delete::release(mRtpInstances[i]);
		}

		if (mRtcpInstances[i]) {
			// delete mRtcpInstances[i];
			Delete::release(mRtcpInstances[i]);
		}
	}
}

// 正常接收到了客户端的请求消息（TCP），进行解析
void RtspConnection::handleReadBytes() {
	bool ret;
	// 每次进行一次的c<->s通信过程，根据tcp/udp传输进行分开处理
	// mInputBuffer是readv从建立连接的connfd接收到的客户端数据
	// tcp传输的时候，客户端和服务器传输的数据都为加上4个字节头数据
	if (mIsRtpOverTcp) {
		if (mInputBuffer.peek()[0] == '$') {
			handleRtpOverTcp();
			return;
		}
	}

	// 通过UDP传输rtp数据，这里开始根据客户端请求解析不同的信息，
	// 分别解析method，url，version，CSeq，rtp和rtsp端口等
	// 如果是SETUP，那么就再解析Transport,提取出客户端的rtp和rtcp端口

	ret = parseRequest();
	if (ret != true) {
		LOG_WARNING("failed to parse request\n"); // 错误解析的把连接加入到断开的列表
		handleDisconnection();
		return;
	}

	bool errOcc = false;

	// 根据解析到的客户端的method回复消息
	// 每次发送消息后，mOutBuffer的mReadIndex，mWriteIndex都会置0
	switch (mMethod) {
	case OPTIONS: // OPTIONS 请求服务器可用方法

		/*
		RTSP/1.0 200 OK\r\n
		CSeq: 1\r\n
		Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n
		\r\n
		*/

		if (handleCmdOption() != true) {
			errOcc = true;
		}
		break;
	case DESCRIBE: // DESCRIBE 得到媒体描述信息
		// S回应SDP格式的媒体描述信息，告诉C当前有哪些音视频流，有什么属性（一个会话级描述，IP、端口等公共的描述，多个媒体级描述，每个音视频流对应一个描述，如编解码器信息等）
		/*
		RTSP/1.0 200 OK\r\n
		CSeq: 2\r\n
		Content-length: 146\r\n
		Content-type: application/sdp\r\n
		\r\n

		v=0\r\n
		o=- 91565340853 1 in IP4 192.168.31.115\r\n
		t=0 0\r\n
		a=contol:*\r\n
		m=video 0 RTP/AVP 96\r\n
		a=rtpmap:96 H264/90000\r\n
		a=framerate:25\r\n
		a=control:track0\r\n
		*/

		if (handleCmdDescribe() != true) {
			errOcc = true;
		}
		break;
	case SETUP: // SETUP 建立 RTSP 会话，创建RTP RTCP的传输
		// C发送建立请求，请求建立连接会话，准备接收音视频数据。transport字段列出可接受的传输选项

		/*
		// UDP
		RTSP/1.0 200 OK\r\n
		CSeq: 3\r\n
		Transport: RTP/AVP;unicast;client_port=54492-54493;server_port=56400-56401\r\n
		Session: 66334873\r\n
		\r\n

		//TCP
		RTSP/1.0 200 OK\r\n
		CSeq: 4\r\n
		Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n
		Session: 327b23c6\r\n
		\r\n
		*/
		if (handleCmdSetup() != true) {
			errOcc = true;
		}
		break;
	case PLAY: // PLAY 请求开始传输数据，传输数据

		/*
		C 请求 S 开始发送数据
		PLAY rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
		CSeq: 4\r\n
		Session: 66334873\r\n
		Range: npt=0.000-\r\n
		\r\n
		*/
		if (handleCmdPlay() != true) {
			errOcc = true;
		}
		break;
	case TEARDOWN:
		if (handleCmdTeardown() != true) {
			errOcc = true;
		}
		break;
	case GET_PARAMETER:
		if (handleCmdGetParamter() != true) {
			errOcc = true;
		}
		break;
	default:
		errOcc = true;
		break;
	}
	// 到这里只是连接的建立
	if (errOcc)
		handleDisconnection();
}

bool RtspConnection::parseRequest() {
	bool ret;

	// 根据\r\n提取一行
	const char *crlf = mInputBuffer.findCRLF();
	// 没有\r\n就是有错，mReadIndex和mWriteIndex都置0归位
	if (crlf == NULL) {
		mInputBuffer.retrieveAll();
		return false;
	}

	// 解析第一行method url vesion，说明有\r\n符号，crlf指向那个位置
	ret = parseRequest1(mInputBuffer.peek(), crlf);
	if (ret == false) {
		mInputBuffer.retrieveAll();
		return false;
	}
	// 接下来要解析序列号
	// 移动读索引mReadIndex到crlf后边两个，这里截取固定长度的
	mInputBuffer.retrieveUntil(crlf + 2);

	// 再找最后一次出现的\r\n的索引
	// 得到这条请求剩余的消息
	crlf = mInputBuffer.findLastCrlf();
	if (crlf == NULL) {
		mInputBuffer.retrieveAll();
		return false;
	}

	// 到这里消息应该是从CSeq那一行开始的，先解析CSeq然后再根据不同的method
	// 进行不同的处理
	ret = parseRequest2(mInputBuffer.peek(), crlf);
	if (ret == false) {
		mInputBuffer.retrieveAll();
		return false;
	}

	mInputBuffer.retrieveUntil(crlf + 2);

	return true;
}

// 解析客户端的请求，begin是这一条消息的开始，end是指向\r\n之前位置的指针
bool RtspConnection::parseRequest1(const char *begin, const char *end) {
	// 取出来消息的第一行，不包含\r\n
	std::string message(begin, end);
	char method[64] = {0};
	char url[512] = {0};
	char version[64] = {0};

	if (sscanf(message.c_str(), "%s %s %s", method, url, version) != 3) {
		return false;
	}

	// printf("method: %s\n", method);
	// std::cout<<"method: "<<method<<std::endl;
	if (!strcmp(method, "OPTIONS"))
		mMethod = OPTIONS;
	else if (!strcmp(method, "DESCRIBE"))
		mMethod = DESCRIBE;
	else if (!strcmp(method, "SETUP"))
		mMethod = SETUP;
	else if (!strcmp(method, "PLAY"))
		mMethod = PLAY;
	else if (!strcmp(method, "TEARDOWN"))
		mMethod = TEARDOWN;
	else if (!strcmp(method, "GET_PARAMETER"))
		mMethod = GET_PARAMETER;
	else {
		mMethod = NONE;
		return false;
	}

	if (strncmp(url, "rtsp://", 7) != 0) {
		return false;
	}

	uint16_t port = 0;
	char ip[64] = {0};
	char suffix[64] = {0};

	if (sscanf(url + 7, "%[^:]:%hu/%s", ip, &port, suffix) == 3) {

	} else if (sscanf(url + 7, "%[^/]/%s", ip, suffix) == 2) {
		port = 554;
	} else {
		return false;
	}

	mUrl = url;
	mSuffix = suffix;

	return true;
}

// 解析CSeq字段
bool RtspConnection::parseCSeq(std::string &message) {
	std::size_t pos = message.find("CSeq");
	if (pos != std::string::npos) {
		uint32_t cseq = 0;
		sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);
		mCSeq = cseq;
		return true;
	}

	return false;
}

// 解析Accept字段
bool RtspConnection::parseAccept(std::string &message) {
	if ((message.rfind("Accept") == std::string::npos)
		|| (message.rfind("sdp") == std::string::npos)) {
		return false;
	}

	return true;
}

// 解析Transport字段
bool RtspConnection::parseTransport(std::string &message) {
	std::size_t pos = message.find("Transport");
	if (pos != std::string::npos) {
		// RTP OVER TCP时候会通过interleaved=0-1表示会话连接的RTP channel为0，RTCP channel为1
		if ((pos = message.find("RTP/AVP/TCP")) != std::string::npos) {
			uint8_t rtpChannel, rtcpChannel;
			mIsRtpOverTcp = true; // 通过TCP传输

			if (sscanf(
					message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hhu-%hhu", &rtpChannel,
					&rtcpChannel)
				!= 2) {
				return false;
			}

			mRtpChannel = rtpChannel;
			return true;
		} else if ((pos = message.find("RTP/AVP")) != std::string::npos) {
			// UDP传输
			uint16_t rtpPort = 0, rtcpPort = 0;

			// 单播解析rtp和rtcp端口
			if (((message.find("unicast", pos)) != std::string::npos)) {
				if (sscanf(
						message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu", &rtpPort, &rtcpPort)
					!= 2) {
					return false;
				}
			} else if ((message.find("multicast", pos)) != std::string::npos) {
				// 多播这里没有处理
				return true;
			} else
				return false;

			mPeerRtpPort = rtpPort;
			mPeerRtcpPort = rtcpPort;
		} else
			return false;

		return true;
	}

	return false;
}

// 解析媒体Track
bool RtspConnection::parseMediaTrack() {
	std::size_t pos = mUrl.find("track0");
	if (pos != std::string::npos) {
		mTrackId = MediaSession::TrackId0;
		return true;
	}

	pos = mUrl.find("track1");
	if (pos != std::string::npos) {
		mTrackId = MediaSession::TrackId1;
		return true;
	}

	return false;
}

// 解析SessionId字段
bool RtspConnection::parseSessionId(std::string &message) {
	std::size_t pos = message.find("Session");
	if (pos != std::string::npos) {
		uint32_t sessionId = 0;
		if (sscanf(message.c_str() + pos, "%*[^:]: %u", &sessionId) != 1)
			return false;
		return true;
	}

	return false;
}

bool RtspConnection::parseRequest2(const char *begin, const char *end) {
	std::string message(begin, end);

	if (parseCSeq(message) != true)
		return false;

	/*
	OPTIONS请求服务器可用方法，不需要处理
	OPTIONS rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
	CSeq: 1\r\n
	\r\n
	*/
	if (mMethod == OPTIONS)
		return true;
	/*
	DESCRIBE请求得到S提供的媒体描述信息
	DESCRIBE rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
	CSeq: 2\r\n
	Accept: application/sdp\r\n
	\r\n
	*/
	if (mMethod == DESCRIBE)
		return parseAccept(message);

	/*
	SETUP 建立 RTSP 会话，准备接收音视频数据。
	// UDP
	SETUP rtsp://192.168.31.115:8554/live/track0 RTSP/1.0\r\n
	CSeq: 3\r\n
	Transport: RTP/AVP;unicast;client_port=54492-54493\r\n
	\r\n

	//TCP
	SETUP rtsp://127.0.0.1:8554/live/track0 RTSP/1.0\r\n
	CSeq: 4\r\n
	Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n
	\r\n
	*/
	// 单播多播的处理，解析rtp和rtcp端口等
	if (mMethod == SETUP) {
		if (parseTransport(message) != true)
			return false;
		// 解析track？？
		return parseMediaTrack();
	}
	/*
	PLAY 请求开始传输数据，传输数据
	PLAY rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
	CSeq: 4\r\n
	Session: 66334873\r\n
	Range: npt=0.000-\r\n
	\r\n
	*/
	if (mMethod == PLAY)
		return parseSessionId(message);

	if (mMethod == TEARDOWN)
		return true;

	if (mMethod == GET_PARAMETER)
		return true;

	return false;
}
// 处理OPTIONS命令
bool RtspConnection::handleCmdOption() {
	// 用一个临时缓冲区来接，然后再append到mOutBuffer，通过buffer的write发送
	snprintf(
		mBuffer, sizeof(mBuffer),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n"
		"\r\n",
		mCSeq);
	// 把mBuffer中的数据添加到mOutBuffer并发送出去
	if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
		return false;

	return true;
}

// 处理DESCRIBE命令
bool RtspConnection::handleCmdDescribe() {
	/* 找到会话 */
	MediaSession *session = mRtspServer->loopupMediaSession(mSuffix);
	if (!session) {
		LOG_DEBUG("can't loop up %s session\n", mSuffix.c_str());
		return false;
	}

	mSession = session;
	/* 获取sdp信息 */
	std::string sdp = session->generateSDPDescription();

	memset((void *)mBuffer, 0, sizeof(mBuffer));
	/* 返回结果 */
	snprintf(
		(char *)mBuffer, sizeof(mBuffer),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %u\r\n"
		"Content-Length: %u\r\n"
		"Content-Type: application/sdp\r\n"
		"\r\n"
		"%s",
		mCSeq, (unsigned int)sdp.size(), sdp.c_str());

	if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
		return false;

	return true;
}
// 处理SETUP命令
bool RtspConnection::handleCmdSetup() {
	char sessionName[100]; // 就是设置的live
	if (sscanf(mSuffix.c_str(), "%[^/]/", sessionName) != 1) {
		return false;
	}

	MediaSession *session = mRtspServer->loopupMediaSession(sessionName);
	if (!session) {
		LOG_DEBUG("can't loop up %s session\n", sessionName);
		return false;
	}

	if (mTrackId >= MEDIA_MAX_TRACK_NUM || mRtpInstances[mTrackId] || mRtcpInstances[mTrackId])
		return false;

	if (session->isStartMulticast()) {
		snprintf(
			(char *)mBuffer, sizeof(mBuffer),
			"RTSP/1.0 200 OK\r\n"
			"CSeq: %d\r\n"
			"Transport: RTP/AVP;multicast;"
			"destination=%s;source=%s;port=%d-%d;ttl=255\r\n"
			"Session: %08x\r\n"
			"\r\n",
			mCSeq, session->getMulticastDestAddr().c_str(), sockets::getLocalIp().c_str(),
			session->getMulticastDestRtpPort(mTrackId),
			session->getMulticastDestRtpPort(mTrackId) + 1, mSessionId);
	} else {
		// rtp over tcp
		if (mIsRtpOverTcp) {

			// 创建rtp over tcp */
			createRtpOverTcp(mTrackId, mSocket.fd(), mRtpChannel);
			mRtpInstances[mTrackId]->setSessionId(mSessionId);
			session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);

			snprintf(
				(char *)mBuffer, sizeof(mBuffer),
				"RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n"
				"Session: %08x\r\n"
				"\r\n",
				mCSeq, mRtpChannel, mRtpChannel + 1, mSessionId);
		} else { // rtp over udp
			// 默认都是通过udp传输的，创建UDP连接
			if (createRtpRtcpOverUdp(mTrackId, mPeerIp, mPeerRtpPort, mPeerRtcpPort) != true) {
				LOG_WARNING("failed to create rtp and rtcp\n");
				return false;
			}

			mRtpInstances[mTrackId]->setSessionId(mSessionId);
			mRtcpInstances[mTrackId]->setSessionId(mSessionId);

			// RtpInstances添加到某个track的列表中
			session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);

			snprintf(
				(char *)mBuffer, sizeof(mBuffer),
				"RTSP/1.0 200 OK\r\n"
				"CSeq: %u\r\n"
				"Transport: RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu\r\n"
				"Session: %08x\r\n"
				"\r\n",
				mCSeq, mPeerRtpPort, mPeerRtcpPort, mRtpInstances[mTrackId]->getLocalPort(),
				mRtcpInstances[mTrackId]->getLocalPort(), mSessionId);
		}
	}

	if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
		return false;

	return true;
}
// 处理PLAY命令
bool RtspConnection::handleCmdPlay() {
	snprintf(
		(char *)mBuffer, sizeof(mBuffer),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"Range: npt=0.000-\r\n"
		"Session: %08x; timeout=60\r\n"
		"\r\n",
		mCSeq, mSessionId);

	if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
		return false;

	for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
		if (mRtpInstances[i])
			mRtpInstances[i]->setAlive(true);

		if (mRtcpInstances[i])
			mRtcpInstances[i]->setAlive(true);
	}

	return true;
}
// 处理TEARDOWN命令
bool RtspConnection::handleCmdTeardown() {
	snprintf(
		(char *)mBuffer, sizeof(mBuffer),
		"RTSP/1.0 200 OK\r\n"
		"CSeq: %d\r\n"
		"\r\n",
		mCSeq);

	if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
		return false;
	}

	return true;
}
// 处理GET_PARAMETER命令
bool RtspConnection::handleCmdGetParamter() {
	// 这里需要实现GET_PARAMETER命令的处理逻辑，根据实际需求进行编写。
	// 目前的代码中，该部分逻辑尚未实现，需要根据具体的需求补充相应的功能。
}

// mOutBuffer是服务端发给客户端的数据，这里是RTSP应答消息
int RtspConnection::sendMessage(void *buf, int size) {
	int ret;

	mOutBuffer.append(buf, size);
	// 通过connfd发送数据调用write，发送的多少是可读数据的多少
	ret = mOutBuffer.write(mSocket.fd());

	// 发送完成之后就要全部归0
	mOutBuffer.retrieveAll();

	return ret;
}

// 发送已有的数据给客户端
int RtspConnection::sendMessage() {
	int ret;

	ret = mOutBuffer.write(mSocket.fd());
	mOutBuffer.retrieveAll();

	return ret;
}

// 创建RTP和RTCP的UDP连接
bool RtspConnection::createRtpRtcpOverUdp(
	MediaSession::TrackId trackId, std::string peerIp, uint16_t peerRtpPort,
	uint16_t peerRtcpPort) {
	int rtpSockfd, rtcpSockfd;
	int16_t rtpPort, rtcpPort;
	bool ret;

	if (mRtpInstances[trackId] || mRtcpInstances[trackId])
		return false;

	int i; // 尝试10次
	for (i = 0; i < 10; ++i) {
		// 创建连接的rtp和rtsp描述符
		rtpSockfd = sockets::createUdpSock();
		if (rtpSockfd < 0)
			return false;

		rtcpSockfd = sockets::createUdpSock();
		if (rtcpSockfd < 0) {
			close(rtpSockfd);
			return false;
		}

		uint16_t port = rand() & 0xfffe;
		// 端口要大
		if (port < 10000)
			port += 10000;

		rtpPort = port;
		rtcpPort = port + 1;

		ret = sockets::bind(rtpSockfd, "0.0.0.0", rtpPort);
		if (ret != true) {
			sockets::close(rtpSockfd);
			sockets::close(rtcpSockfd);
			continue;
		}

		ret = sockets::bind(rtcpSockfd, "0.0.0.0", rtcpPort);
		if (ret != true) {
			sockets::close(rtpSockfd);
			sockets::close(rtcpSockfd);
			continue;
		}

		break;
	}

	if (i == 10)
		return false;

	mRtpInstances[trackId] = RtpInstance::createNewOverUdp(rtpSockfd, rtpPort, peerIp, peerRtpPort);
	mRtcpInstances[trackId] = RtcpInstance::createNew(rtcpSockfd, rtcpPort, peerIp, peerRtcpPort);

	return true;
}
// 创建RTP over TCP连接
bool RtspConnection::createRtpOverTcp(
	MediaSession::TrackId trackId, int sockfd, uint8_t rtpChannel) {
	mRtpInstances[trackId] = RtpInstance::createNewOverTcp(sockfd, rtpChannel);

	return true;
}
// 处理RTP over TCP数据
void RtspConnection::handleRtpOverTcp() {
	uint8_t *buf = (uint8_t *)mInputBuffer.peek();
	uint16_t size;

	size = (buf[2] << 8) | buf[3];
	if (mInputBuffer.readableBytes() < size + 4)
		return;

	mInputBuffer.retrieve(size + 4);
}