#include "yamlnode_p.h"
#include <yamltool/yamlnode.h>

namespace YamlTool
{
	YamlNode ::YamlNodePrivate::YamlNodePrivate()
	{
		m_node = YAML::Node();
	}
	YamlNode::YamlNodePrivate::YamlNodePrivate(const YAML::Node& node)
	{
		m_node = node;
	}
	YAML::Node& YamlNode::YamlNodePrivate::node()
	{
		return m_node;
	}
}// namespace Config
