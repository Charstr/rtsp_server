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

Logger::Logger() : mCurPtr(mData) {}

Logger::~Logger() {
	if (mIsStdout)
		printf("%s", mData);
	else
		AsyncLogging::instance()->append(mData, mCurPtr - mData);
}

void Logger::setLogFile(std::string file) {
	Logger::mLogFile = file;
	if (Logger::mLogFile == "/dev/stdout")
		Logger::mIsStdout = true;
	else
		Logger::mIsStdout = false;
}

std::string Logger::getLogFile() { return Logger::mLogFile; }

void Logger::setLogLevel(LogLevel level) { Logger::mLogLevel = level; }

Logger::LogLevel Logger::getLogLevel() { return Logger::mLogLevel; }

void Logger::write(Logger::LogLevel level, const char *file, const char *func, int line,
				   const char *format, ...) {
	if (level > Logger::mLogLevel)
		return;

	struct timeval now = {0, 0};
	gettimeofday(&now, NULL);
	struct tm *sysTime = localtime(&(now.tv_sec));

	mThisLogLevel = level;
	// 构造日志的时间戳
	sprintf(mCurPtr, "%d-%02d-%02d %02d:%02d:%02d", sysTime->tm_year + 1900, sysTime->tm_mon + 1,
			sysTime->tm_mday, sysTime->tm_hour, sysTime->tm_min, sysTime->tm_sec);
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
	mCurPtr += strlen(mCurPtr);
	// 记录文件名、函数名和行号
	sprintf(mCurPtr, "%s:%s:%d ", file, func, line);
	mCurPtr += strlen(mCurPtr);
	// 使用可变参数列表进行格式化输出
	va_list valst;
	va_start(valst, format);

	vsnprintf(mCurPtr, sizeof(mData) - (mCurPtr - mData), format, valst);

	va_end(valst);

	mCurPtr += strlen(mCurPtr);
}