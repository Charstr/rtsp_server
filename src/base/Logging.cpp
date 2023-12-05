#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "AsyncLogging.h"
#include "Logging.h"

Logger::LogLevel Logger::mLogLevel = Logger::LogDebug;
std::string Logger::mLogFile = "/dev/stdout";
bool Logger::mIsStdout = true;

// 每次写的时候都初始化为mData的起始地址，然后记录地址进行偏移
Logger::Logger() : mCurPtr(mData) {}

Logger::~Logger() {
	if (mIsStdout)
		printf("%s", mData);
	else
		// 异步要写的是mData数组中mCurPtr - mData长度的数据
		// 析构的时候调用异步写程序
		AsyncLogging::instance()->append(mData, mCurPtr - mData);
}

void Logger::setLogFile(std::string file) {
	Logger::mLogFile = file;
	// 指定了输出文件就不是标准输出
	if (Logger::mLogFile == "/dev/stdout")
		Logger::mIsStdout = true;
	else
		Logger::mIsStdout = false;
}

std::string Logger::getLogFile() {
	return Logger::mLogFile;
}

void Logger::setLogLevel(LogLevel level) {
	Logger::mLogLevel = level;
}

Logger::LogLevel Logger::getLogLevel() {
	return Logger::mLogLevel;
}

// 完成一次日志的构造
void Logger::write(
	Logger::LogLevel level, const char *file, const char *func, int line, const char *format, ...) {
	if (level > Logger::mLogLevel)
		return;

	struct timeval now = {0, 0};
	gettimeofday(&now, NULL);
	struct tm *sysTime = localtime(&(now.tv_sec));

	mThisLogLevel = level;

	// 构造日志的时间戳
	sprintf(
		mCurPtr, "%d-%02d-%02d %02d:%02d:%02d", sysTime->tm_year + 1900, sysTime->tm_mon + 1,
		sysTime->tm_mday, sysTime->tm_hour, sysTime->tm_min, sysTime->tm_sec);
	// mCurPtr每一次格式化写进去的时候，都是新的一段，所以
	// 在mCurPtr += strlen(mCurPtr);偏移的其实是新的长度

	mCurPtr += strlen(mCurPtr);
	// 根据日志级别设置标签
	if (level == Logger::LogDebug) {
		sprintf(mCurPtr, " <DEBUG> ");
	} else if (level == Logger::LogWarning) {
		sprintf(mCurPtr, " <WARNING> ");
	} else if (level == Logger::LogError) {
		sprintf(mCurPtr, " <ERROR> ");
	} else {
		return;
	}
	// 再次偏移
	mCurPtr += strlen(mCurPtr);
	// 格式化输出记录文件名、函数名和行号
	sprintf(mCurPtr, "%s:%s:%d ", file, func, line);
	mCurPtr += strlen(mCurPtr);

	// 使用可变参数列表处理日志信息的格式化和填充
	// valst指向format 后面的第一个可变参数
	va_list valst;
	va_start(valst, format);

	// 使用可变参数列表来填充格式化字符串
	// 把传进来的format, ...经过格式化之后写到mCurPtr 指向的位置，
	// 可写的长度是剩余缓冲区的大小为sizeof(mData) - (mCurPtr - mData)
	vsnprintf(mCurPtr, sizeof(mData) - (mCurPtr - mData), format, valst);

	va_end(valst);
	// 再次偏移
	mCurPtr += strlen(mCurPtr);
}