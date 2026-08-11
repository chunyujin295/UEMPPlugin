#include <logger/logger.h>
#include <logger_p.h>

void Logger::setConfigPath(const std::string& configFilePath, bool isDeleteOldConfig)
{
	LogPrivate::setConfigPath(configFilePath, isDeleteOldConfig);
}

void Logger::trace(const char* fileName, const int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	LogPrivate::trace(fileName, fileLine, function, msgList);
}

void Logger::debug(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	LogPrivate::debug(fileName, fileLine, function, msgList);
}

void Logger::info(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	LogPrivate::info(fileName, fileLine, function, msgList);
}

void Logger::warn(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	LogPrivate::warn(fileName, fileLine, function, msgList);
}

void Logger::error(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	LogPrivate::error(fileName, fileLine, function, msgList);
}

void Logger::critical(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	LogPrivate::critical(fileName, fileLine, function, msgList);
}

std::string Logger::addCallBack(const std::function<void(const LogMsg& logMsg)>& logCallBack, LogLevel level)
{
	return LogPrivate::addCallBackSink( logCallBack, level);
}
void Logger::removeCallBack(const std::string& sinkId)
{
	LogPrivate::removeCallBackSink(sinkId);
}

void Logger::shutdown()
{
	LogPrivate::shutdown();
}