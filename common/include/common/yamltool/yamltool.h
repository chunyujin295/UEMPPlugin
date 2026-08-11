/*************************************************
  * 描述：yaml读取工具类
  *
  * File：yamltool.h
  * Author：3029795434@qq.com
  * Date：2025/9/2
  * Update：
  * ************************************************/
#ifndef YAMLTOOL_H
#define YAMLTOOL_H

#include <any>
#include "export.h"
#include "yamlnode.h"
#include <functional>

namespace YamlTool
{
	class YAML_TOOL_API YamlTool
	{
	public:
		/**
		* @brief 加载配置文件
		* @param configFilePath 配置文件路径
		 */
		static bool loadFile(YamlNode& node, const std::string& configFilePath);


		/**
		 * @brief 创建配置文件
		 * @param filePath 文件路径
		 */
		static void saveAsFile(const YamlNode& node, const std::string& filePath = "config.yaml");


		/**
		 * 获取节点
		 * @param node
		 * @param nodeName
		 * @return
		 */
		static YamlNode getNode(const YamlNode& node, const std::string& nodeName);


		/**
		 * @brief 获取列表中的节点
		 * @param node
		 * @param index
		 * @return
		 */
		static YamlNode getSequenceNode(const YamlNode& node, std::size_t index);


		/**
		 * @brief 安全取可选节点（默认值）
		 * @tparam T
		 * @param node
		 * @param key
		 * @param def
		 * @return
		 */
		template<typename T>
		static T getDef(const YamlNode& node, const std::string& key, const T& def = T())
		{
			auto anyVal = _getDef(node, key, def);

			if (anyVal.has_value())
			{
				try
				{
					return std::any_cast<T>(anyVal);
				}
				catch (...)
				{
					return def;
				}
			}
			return def;
		}

		/**
		 * @brief 强制写入节点（覆盖已有值）
	     * @tparam T  可序列化到 YAML 的类型
	     * @param node  被写入的 Node
	     * @param key   键名
	     * @param value 要写入的值
	     */
		template<typename T>
		static void set(YamlNode& node, const std::string& key, const T& value)
		{
			_set(node, key, value);
		}

		/**
		 * @brief 安全写入可选节点（仅当不存在或类型不匹配时才写入）
		 * @tparam T  可序列化到 YAML 的类型
		 * @param node  被写入的 Node
		 * @param key   键名
		 * @param value 默认值
		 * @return bool 是否真正发生了写入
		 */
		template<typename T>
		static bool setDef(YamlNode& node, const std::string& key, const T& value)
		{
			return _setDef(node, key, value);
		}

		static void addNode(YamlNode& node, const std::string& key, const YamlNode& value);

		/**
		 * @brief 追加到列表
		 * @param node
		 * @param value
		 * @return
		 */
		static void pushBack(YamlNode& node, const YamlNode& value);


		/**
	     * 1. 强制写入空节点（null）
	     * 无论节点是否存在，有则改写，无则创建
	     */
		static void setNull(YamlNode& node, const std::string& key);

		/**
		 * 2. 仅当节点不存在时才写入空节点
		 * 当节点存在时则不进行处理
		 * @return bool 是否真正发生了写入
		 */
		static bool setNullDef(YamlNode& node, const std::string& key);

		// void createInputConfig();
		//
		// void createInputConfigCommon(YAML::Node& node);

	private:
		static std::any _getDef(const YamlNode& node, const std::string& key, const std::any& def);
		static void _set(YamlNode& node, const std::string& key, const std::any& value);
		static bool _setDef(YamlNode& node, const std::string& key, const std::any& value);
	};

}// namespace Config

#endif//YAMLTOOL_H
