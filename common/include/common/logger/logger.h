#ifndef LOGGER_H
#define LOGGER_H

#include <any>
#include "export.h"
#include <functional>
#include <string>

#ifndef QT_NO_DEBUG// 如果debug模式，则应声明DEBUG宏，用来判断是否启用日志输出
#define DEBUG
#endif


enum class LogLevel : int
{
	Trace = 0,
	Debug = 1,
	Info = 2,
	Warn = 3,
	Error = 4,
	Critical = 5
};

struct LogMsg
{
	std::string fileName;
	std::string codeLine;
	std::string funcName;
	std::string threadId;
	LogLevel level;
	std::string msgFormatted;
	std::string msg;
};


class LOGGER_API Logger// PIMPL模式，但没有对象指针，因为对外接口都是静态的
{
public:
	/**
	 * 手动修改读取配置文件的路径
	 * 若不调用此方法修改，默认读取位置为可执行程序所在路径下
	 * @param configFilePath 配置文件路径
	 * @param isDeleteOldConfig 是否删除旧的配置文件
	 */
	static void setConfigPath(const std::string& configFilePath, bool isDeleteOldConfig = true);

	/**
     * 输出带行号的trace级别日志文本，严重级别0
     * fileName,fileLine,funtion参数可使用宏定义GET_LINE取代
     * @param fileName 日志输出位置文件名，应传入宏__FILE__
     * @param fileLine 日志输出位置行号，应传入宏__LINE__
     * @param function 日志输出所在函数，应传入宏__FUNCTION__
     * @param msgList 日志文本信息列表
     */
	static void trace(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);

	/**
     * 输出带行号的debug级别日志文本，严重级别1
     * fileName,fileLine,funtion参数可使用宏定义GET_LINE取代
     * @param fileName 日志输出位置文件名，应传入宏__FILE__
     * @param fileLine 日志输出位置行号，应传入宏__LINE__
     * @param function 日志输出所在函数，应传入宏__FUNCTION__
     * @param msgList 日志文本信息列表
     */
	static void debug(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);

	/**
     * 输出带行号的info级别日志文本，严重级别2
     * fileName,fileLine,funtion参数可使用宏定义GET_LINE取代
     * @param fileName 日志输出位置文件名，应传入宏__FILE__
     * @param fileLine 日志输出位置行号，应传入宏__LINE__
     * @param function 日志输出所在函数，应传入宏__FUNCTION__
     * @param msgList 日志文本信息列表
     */
	static void info(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);


	/**
     * 输出带行号的warn级别日志文本，严重级别3
     * fileName,fileLine,funtion参数可使用宏定义GET_LINE取代
     * @param fileName 日志输出位置文件名，应传入宏__FILE__
     * @param fileLine 日志输出位置行号，应传入宏__LINE__
     * @param function 日志输出所在函数，应传入宏__FUNCTION__
     * @param msgList 日志文本信息列表
     */
	static void warn(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);


	/**
     * 输出带行号的error级别日志文本，严重级别4
     * fileName,fileLine,funtion参数可使用宏定义GET_LINE取代
     * @param fileName 日志输出位置文件名，应传入宏__FILE__
     * @param fileLine 日志输出位置行号，应传入宏__LINE__
     * @param function 日志输出所在函数，应传入宏__FUNCTION__
     * @param msgList 日志文本信息列表
     */
	static void error(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);


	/**
	 * 输出带行号的critical级别日志文本，严重级别5
	 * fileName,fileLine,funtion参数可使用宏定义GET_LINE取代
	 * @param fileName 日志输出位置文件名，应传入宏__FILE__
	 * @param fileLine 日志输出位置行号，应传入宏__LINE__
	 * @param function 日志输出所在函数，应传入宏__FUNCTION__
	 * @param msgList 日志文本信息列表
	 */
	static void critical(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);


	/**
	 * 设置日志回调函数，每当日志输出时，调用回调函数
	 * @param logCallBack 日志回调函数
	 * @param level 日志级别
	 * @return sinkId 日志回调id
	 */
	static std::string addCallBack(const std::function<void(const LogMsg& logMsg)>& logCallBack, LogLevel level = LogLevel::Trace);

	/**
	 * 移除日志回调函数
	 * @param sinkId 回调日志id
	 */
	static void removeCallBack(const std::string& sinkId);

	/**
	 * 关闭日志系统，等待异步队列排空后释放所有资源
	 * 应在 main() 结束前调用，确保所有日志被写出
	 */
	static void shutdown();
};

// 通过宏定义方式调用日志输出：
#define GET_LINE __FILE__, __LINE__, __FUNCTION__// 宏，用来代替位置、行号、函数信息这三个宏

#define LOG_SET_CONFIG_PATH(...) Logger::setConfigPath(__VA_ARGS__)
#define LOG_TRACE(...) Logger::trace(GET_LINE, {__VA_ARGS__})   // 日志宏，[trace级别]
#define LOG_DEBUG(...) Logger::debug(GET_LINE, {__VA_ARGS__})   // 同上 [debug级别]
#define LOG_INFO(...) Logger::info(GET_LINE, {__VA_ARGS__})     // 同上 [info级别]
#define LOG_WARN(...) Logger::warn(GET_LINE, {__VA_ARGS__})     // 同上 [warn级别]
#define LOG_ERROR(...) Logger::error(GET_LINE, {__VA_ARGS__})   // 同上 [error级别]
#define LOG_CRITI(...) Logger::critical(GET_LINE, {__VA_ARGS__})// 同上 [critical级别]

#endif//LOGGER_H
