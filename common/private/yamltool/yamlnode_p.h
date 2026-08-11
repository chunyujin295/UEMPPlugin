#ifndef YAMLNODEPIMPL_H
#define YAMLNODEPIMPL_H

#include <yamltool/yamlnode.h>
#include <yaml-cpp/yaml.h>

namespace YamlTool
{
	class YamlNode::YamlNodePrivate
	{
		friend class YamlNode;

	public:
		YamlNodePrivate();
		explicit YamlNodePrivate(const YAML::Node& node);

		YAML::Node& node();

	private:
		YAML::Node m_node;
	};
}// namespace Config

#endif//YAMLNODEPIMPL_H
