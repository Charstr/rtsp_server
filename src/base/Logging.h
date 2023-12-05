#ifndef _LOGGING_H_
#define _LOGGING_H_
#include <string>

class Logger;
// 定义日志级别的宏，根据不同的日志级别来记录日志
// __FILE__, __FUNCTION__, __LINE__ 这个宏是获取哪一个文件的哪个函数的第几行
// __VA_ARGS__是可变参数
// format是LOG_ERROR("xxx")中的这个字符串

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

public:
	// 日志级别
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

	// 静态的，设置了之后，对象销毁也能正常获取到的
	static LogLevel mLogLevel; // 静态成员变量，用于记录全局日志级别
	static std::string mLogFile; // 静态成员变量，用于记录全局日志文件路径
	static bool mIsStdout; // 静态成员变量，标志是否输出到标准输出
};

#endif //_LOGING_H_