/*************************************************
  * 描述：日志私有实现类
  *
  * File：logger_p.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2025/9/2
  * Update：
  * ************************************************/
#ifndef COREXI_COMMON_PC_LOGGER_P_H
#define COREXI_COMMON_PC_LOGGER_P_H
#include "id8generator.hpp"
#include <logger/logger.h>
#include <memory>
#include <spdlog/spdlog.h>

class LogPrivate
{
public:
	/**
     * 删除拷贝构造，确保单例
     */
	LogPrivate(const LogPrivate&) = delete;

	/**
	 * 删除移动构造，确保单例
	 */
	LogPrivate(const LogPrivate&&) = delete;

	/**
     * 删除赋值符号重载，确保单例
     * @return
     */
	LogPrivate& operator=(const LogPrivate&) = delete;
	LogPrivate& operator=(const LogPrivate&&) = delete;


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
     * 创建一个输出到流的日志输出器，并添加到日志对象中
     * 值得注意的是，这种方式不支持输出线程号
     * @param stream 要输出到的流对象
     * @param flush 是否立即刷新流缓冲区
     * @param level 日志级别
     */
	static void setStreamOutPut(std::ostringstream& stream, bool flush = false, LogLevel level = LogLevel::Trace);

	/**
	 * 添加日志回调函数，每当日志输出时，调用回调函数
	 * @param logCallBack 日志回调函数
	 * @param level 日志级别
	 * @return 回调日志id
	 */
	static std::string addCallBackSink(const std::function<void(const LogMsg& logMsg)>& logCallBack, LogLevel level = LogLevel::Trace);

	/**
	 * 删除日志回调函数
	 * @param sinkId 回调日志id
	 */
	static void removeCallBackSink(const std::string& sinkId);

	/**
	 * 关闭日志系统，等待异步队列排空后释放所有资源
	 * 应在 main() 结束前调用，确保所有日志被写出
	 */
	static void shutdown();

private:
	/**
     * 构造
     */
	LogPrivate();

	/**
     * 析构，默认
     */
	~LogPrivate() = default;

	/**
     * 进行初始化
     * @return
     */
	static LogPrivate& getInstance();

	/**
     * 获取内部日志对象spdlog指针
     * @return
     */
	std::shared_ptr<spdlog::logger> getLogger();

	/**
     * 加载指定路径下的日志配置文件
     */
	void loadConfigFile(const std::string& configFilePath);

	/**
     * 创建和加载默认配置
     */
	void loadDefaultConfig(const std::string& configFilePath);

	/**
	 * 删除旧的配置文件
	 * @param configFilePath
	 */
	void deleteOldConfig(const std::string& configFilePath);

	/**
     * 子程序，用于检测ini中属性对应值是否为空，为空则打印错误并返回默认属性值
     * @param itemValue 属性值
     * @param itemName 属性名
     * @param defaultValue 默认属性值
     * @return
     */
	std::string getItemValue(const std::string& itemValue, const std::string& itemName,
							 const std::string& defaultValue);

	/**
     * 子程序，用于检测sink输出路径是否存在，不存在则打印错误
     * @param sinkType
     * @param filePath
     */
	bool checkSinkFilePath(const std::string& sinkType, const std::string& filePath);

	/**
	 * 日志输出公共实现
	 */
	static void logImpl(const char* fileName, int fileLine, const char* function,
						const std::initializer_list<std::any>& msgList,
						bool showLine, spdlog::level::level_enum level);

	/**
	 * 用于拼接字符串
	 */
	static std::string linkString(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList);

	/**
	 * 浮点数转字符串,保留7位有效数字
	 * @param value
	 * @return
	 */
	static std::string floatingNumToString(double value);

	/**
	 * std::any数据转string
	 * @param fileName
	 * @param fileLine
	 * @param function
	 * @param data
	 * @return
	 */
	static std::string anyToString(const char* fileName, int fileLine, const char* function, const std::any& data);

private:
	// spdlog库logger类对象指针
	std::shared_ptr<spdlog::logger> m_logger;

	static std::string m_configFilePath;

	static bool m_traceShowLine;
	static bool m_debugShowLine;
	static bool m_infoShowLine;
	static bool m_warnShowLine;
	static bool m_errorShowLine;
	static bool m_criticalShowLine;

	static std::unordered_map<std::string, std::shared_ptr<spdlog::sinks::sink>> m_callbackSinks;

	static ID8Generator m_id8Generator;
};

#endif//COREXI_COMMON_PC_LOGGER_P_H
