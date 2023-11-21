#ifndef _LOGGING_H_
#define _LOGGING_H_
#include <string>

class Logger;
// 定义日志级别的宏，根据不同的日志级别来记录日志
#define LOG_ERROR(format, ...)                                                                     \
	if (Logger::LogError <= Logger::getLogLevel())                                                 \
	Logger().write(Logger::LogError, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define LOG_WARNING(format, ...)                                                                   \
	if (Logger::LogWarning <= Logger::getLogLevel())                                               \
	Logger().write(Logger::LogWarning, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

#define LOG_DEBUG(format, ...)                                                                     \
	if (Logger::LogDebug <= Logger::getLogLevel())                                                 \
	Logger().write(Logger::LogDebug, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)

class Logger {
	/*
	在不同的日志级别下记录不同的消息，可以将日志写入文件或标准输出。可以通过设置日志级别和日志文件路径来自定义日志的行为。通过调用LOG_ERROR、LOG_WARNING和LOG_DEBUG宏，可以方便地记录不同级别的日志消息。Logger类还支持在日志消息中添加时间戳、文件名、函数名和行号等信息，以方便调试和故障排查。

	*/
public:
	// 定义日志级别的枚举类型
	enum LogLevel {
		LogError,
		LogWarning,
		LogDebug
	};

	Logger();
	~Logger();

	static void setLogFile(std::string file);
	static std::string getLogFile();
	static void setLogLevel(LogLevel level);
	static LogLevel getLogLevel();
	// 将日志信息写入文件或标准输出
	void
	write(LogLevel level, const char *file, const char *func, int line, const char *format, ...);

private:
	char mData[4096]; // 存储日志数据的缓冲区
	char *mCurPtr; // 当前可写入位置的指针
	LogLevel mThisLogLevel; // 当前日志级别

	static LogLevel mLogLevel; // 静态成员变量，用于记录全局日志级别
	static std::string mLogFile; // 静态成员变量，用于记录全局日志文件路径
	static bool mIsStdout; // 静态成员变量，标志是否输出到标准输出
};

#endif //_LOGING_H_