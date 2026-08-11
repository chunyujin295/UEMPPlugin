#ifndef COREXI_COMMON_PC_YAMLTOOL_P_H
#define COREXI_COMMON_PC_YAMLTOOL_P_H

#include <yamltool/yamltool.h>
#include <mutex>
#include <yaml-cpp/yaml.h>

// 用于读取和写入YAML::Node
// 进行了模板与std::any的转换
class TypeRegistry
{
public:
	// 注册类型读写函数，需要提前进行各种类型注册，否则注册表为空的，没什么用
	template<typename T>
	static void registerType()
	{
		// 向哈希表里面存储键值对
		getReaders()[typeid(T)] = [](const YAML::Node& node) -> std::any {
			return node.as<T>();// 如果转换失败，会抛出异常，外部捕获可知是否可以读取对应类型
		};
		getWriters()[typeid(T)] = [](YAML::Node& node, const std::string& key, const std::any& value) {
			node[key] = std::any_cast<T>(value);
		};
	}

	// 从 YAML::Node 读取 std::any
	static std::any read(const YAML::Node& node, const std::type_info& ti);

	// 把 std::any 写入 YAML::Node
	static void write(YAML::Node& node, const std::string& key, const std::any& value);

private:
	using ReaderFunc = std::function<std::any(const YAML::Node&)>;
	using WriterFunc = std::function<void(YAML::Node&, const std::string& key, const std::any&)>;

	static std::unordered_map<std::type_index, ReaderFunc>& getReaders();

	static std::unordered_map<std::type_index, WriterFunc>& getWriters();
};


class YamlToolPrivate
{
public:
	/**
	 * 删除拷贝构造，确保单例
	 */
	YamlToolPrivate(const YamlToolPrivate&) = delete;

	/**
	 * 删除移动构造，确保单例
	 */
	YamlToolPrivate(YamlToolPrivate&&) = delete;

	/**
	 * 删除赋值符号重载，确保单例
	 */
	YamlToolPrivate& operator=(const YamlToolPrivate&) = delete;
	YamlToolPrivate& operator=(const YamlToolPrivate&&) = delete;

	/**
	 * 单例调用
	 * @return
	 */
	static YamlToolPrivate* getInstance();

	/**
	 * 加载文件
	 * @param node
	 * @param configFilePath
	 * @return
	 */
	bool loadFile(YamlTool::YamlNode& node, const std::string& configFilePath);

	/**
	 * 保存文件
	 * @param node
	 * @param filePath
	 */
	void saveAsFile(const YAML::Node& node, const std::string& filePath = "config.yaml");

	/**
	 * 获取节点
	 * @param node
	 * @param nodeName
	 * @return
	 */
	YAML::Node getNode(const YAML::Node& node, const std::string& nodeName);

	/**
	 * 获取序列节点
	 * @param node
	 * @param index
	 * @return
	 */
	YAML::Node getSequenceNode(const YAML::Node& node, std::size_t index);

	/**
	 * 添加节点
	 * @param node
	 * @param key
	 * @param value
	 */
	void addNode(YAML::Node& node, const std::string& key, const YAML::Node& value);

	/**
	 * 添加节点
	 * @param node
	 * @param value
	 */
	void pushBack(YAML::Node& node, const YAML::Node& value);

	/**
	 * 1. 强制写入空节点（null）
	 * 无论节点是否存在，有则改写，无则创建
	 */
	void setNull(YAML::Node& node, const std::string& key);

	/**
	 * 2. 仅当节点不存在时才写入空节点
	 * 当节点存在时则不进行处理
	 * @return bool 是否真正发生了写入
	 */
	bool setNullDef(YAML::Node& node, const std::string& key);

	/**
	 * 安全取可选节点（默认值）
	 * @param node
	 * @param key
	 * @param def
	 * @return
	 */
	std::any _getDef(const YAML::Node& node, const std::string& key, const std::any& def);

	/**
	 * 强制写入节点（覆盖已有值）
	 * @param node
	 * @param key
	 * @param value
	 */
	void _set(YAML::Node& node, const std::string& key, const std::any& value);

	/**
	 * 安全写入可选节点（仅当不存在或类型不匹配时才写入）
	 * @param node
	 * @param key
	 * @param value
	 * @return 是否真正发生了写入
	 */
	bool _setDef(YAML::Node& node, const std::string& key, const std::any& value);

private:
	YamlToolPrivate();
	~YamlToolPrivate() = default;

private:
	// 静态互斥锁，用于确保线程安全
	static std::mutex s_mutex;
	// 静态对象指针
	static YamlToolPrivate* s_instance;
};

#endif//COREXI_COMMON_PC_YAMLTOOL_P_H
