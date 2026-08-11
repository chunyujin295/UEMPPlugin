#include "yamltool_p.h"
#include <fstream>

YamlToolPrivate* YamlToolPrivate::s_instance = nullptr;
std::mutex YamlToolPrivate::s_mutex;

std::any TypeRegistry::read(const YAML::Node& node, const std::type_info& ti)
{
	auto it = getReaders().find(ti);
	if (it != getReaders().end())
	{
		return it->second(node);
	}
	throw std::runtime_error("unsupported type in read: " + std::string(ti.name()));
}
void TypeRegistry::write(YAML::Node& node, const std::string& key, const std::any& value)
{
	auto it = getWriters().find(std::type_index(value.type()));
	if (it != getWriters().end())
	{
		it->second(node, key, value);
	}
	else
	{
		throw std::runtime_error("unsupported type in write: " + std::string(value.type().name()));
	}
}
std::unordered_map<std::type_index, TypeRegistry::ReaderFunc>& TypeRegistry::getReaders()
{
	static std::unordered_map<std::type_index, ReaderFunc> readers;
	return readers;
}
std::unordered_map<std::type_index, TypeRegistry::WriterFunc>& TypeRegistry::getWriters()
{
	static std::unordered_map<std::type_index, WriterFunc> writers;
	return writers;
}


YamlToolPrivate* YamlToolPrivate::getInstance()
{
	// DCL双检锁创建单例
	if (s_instance == nullptr)
	{
		std::lock_guard<std::mutex> lock(s_mutex);// 确保多线程创建单例时线程安全
		if (s_instance == nullptr)
		{
			s_instance = new YamlToolPrivate();
		}
	}
	// 初始化异步线程池
	// spdlog::init_thread_pool(8192, 1);
	return s_instance;
}

bool YamlToolPrivate::loadFile(YamlTool::YamlNode& node, const std::string& configFilePath)
{
	try
	{
		node = YamlTool::YamlNode(configFilePath);
	}
	catch (std::exception& e)
	{
		return false;
	}
	// 空文档检查
	if (!node.isDefined())
	{
		return false;
	}
	return true;
}
void YamlToolPrivate::saveAsFile(const YAML::Node& node, const std::string& filePath)
{
	if (!node.IsDefined() || node.IsNull())
	{
		throw std::runtime_error("failed to save config file: node is invalid");
	}
	try
	{
		std::ofstream fout(filePath);
		fout << node;
		fout.close();
	}
	catch (std::exception& e)
	{
		throw std::runtime_error("failed to save config file: " + filePath + "error: " + e.what());
	}
}
YAML::Node YamlToolPrivate::getNode(const YAML::Node& node, const std::string& nodeName)
{
	if (!node.IsNull() && node.IsDefined())
	{
		if (node[nodeName])
		{
			return node[nodeName];
		}
	}
	return {};
}
YAML::Node YamlToolPrivate::getSequenceNode(const YAML::Node& node, std::size_t index)
{
	if (!node.IsNull() && node.IsDefined() && node.IsSequence() && index < node.size())
	{
		if (node[index])
		{
			return node[index];
		}
	}
	return {};
}
void YamlToolPrivate::addNode(YAML::Node& node, const std::string& key, const YAML::Node& value)
{
	if (!node.IsDefined() || !value.IsDefined())// 如果节点为空或未定义
	{
		throw std::runtime_error("invalid node");
	}
	node[key] = value;
}
void YamlToolPrivate::pushBack(YAML::Node& node, const YAML::Node& value)
{
	if (!node.IsDefined() || !value.IsDefined())// 如果节点为空或未定义
	{
		throw std::runtime_error("invalid node");
	}
	node.push_back(value);
}
void YamlToolPrivate::setNull(YAML::Node& node, const std::string& key)
{
	node[key] = YAML::Node();
}
bool YamlToolPrivate::setNullDef(YAML::Node& node, const std::string& key)
{
	if (node[key])
	{
		return false;
	}

	node[key] = YAML::Node();
	return true;
}
std::any YamlToolPrivate::_getDef(const YAML::Node& node, const std::string& key, const std::any& def)
{
	try
	{
		if (node[key].IsNull() || !node[key].IsDefined())
		{
			return def;
		}
		return TypeRegistry::read(node[key], def.type());
	}
	catch (const YAML::InvalidNode&)
	{
		return def;
	}
	catch (const YAML::BadConversion& e)
	{
		throw std::runtime_error("wrong type for key: " + key + " (" + e.what() + ")");
	}
}
void YamlToolPrivate::_set(YAML::Node& node, const std::string& key, const std::any& value)
{
	if (!node.IsDefined())// 如果节点为空或未定义
	{
		throw std::runtime_error("invalid node");
	}
	TypeRegistry::write(node, key, value);
}
bool YamlToolPrivate::_setDef(YAML::Node& node, const std::string& key, const std::any& value)
{
	// 已存在且类型正确则跳过
	if (node[key] && node[key].Type() == YAML::NodeType::Scalar)
	{
		try
		{
			TypeRegistry::read(node[key], value.type());// 尝试按照给定类型读取，失败会抛异常
			return false;                                                   // 已有有效值，不覆盖
		}
		catch (...)
		{
			/* 类型不匹配，继续向下写 */
			throw std::runtime_error("wrong type for key: " + key);
		}
	}

	// 不存在或类型不匹配，写入
	_set(node, key, value);
	return true;
}

YamlToolPrivate::YamlToolPrivate()
{
	TypeRegistry::registerType<int>();
	TypeRegistry::registerType<bool>();
	TypeRegistry::registerType<double>();
	TypeRegistry::registerType<std::string>();
}