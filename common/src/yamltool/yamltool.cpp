#include <yamltool/yamltool.h>
#include <fstream>
#include <yamlnode_p.h>
#include <yamltool_p.h>

namespace YamlTool
{
	bool YamlTool::loadFile(YamlNode& node, const std::string& configFilePath)
	{
		return YamlToolPrivate::getInstance()->loadFile(node, configFilePath);
	}
	void YamlTool::saveAsFile(const YamlNode& node, const std::string& filePath)
	{
		YamlToolPrivate::getInstance()->saveAsFile(node.m_pImplNode->node(), filePath);
	}
	YamlNode YamlTool::getNode(const YamlNode& node, const std::string& nodeName)
	{
		return YamlNode(YamlNode::YamlNodePrivate(YamlToolPrivate::getInstance()->getNode(node.m_pImplNode->node(), nodeName)));
	}
	YamlNode YamlTool::getSequenceNode(const YamlNode& node, std::size_t index)
	{
		return YamlNode(YamlNode::YamlNodePrivate(YamlToolPrivate::getInstance()->getSequenceNode(node.m_pImplNode->node(), index)));
	}
	void YamlTool::addNode(YamlNode& node, const std::string& key, const YamlNode& value)
	{
		YamlToolPrivate::getInstance()->addNode(node.m_pImplNode->node(), key, value.m_pImplNode->node());
	}
	void YamlTool::pushBack(YamlNode& node, const YamlNode& value)
	{
		YamlToolPrivate::getInstance()->pushBack(node.m_pImplNode->node(), value.m_pImplNode->node());
	}

	void YamlTool::setNull(YamlNode& node, const std::string& key)
	{
		YamlToolPrivate::getInstance()->setNull(node.m_pImplNode->node(), key);
	}

	bool YamlTool::setNullDef(YamlNode& node, const std::string& key)
	{
		return YamlToolPrivate::getInstance()->setNullDef(node.m_pImplNode->node(), key);
	}

	std::any YamlTool::_getDef(const YamlNode& node, const std::string& key, const std::any& def)
	{
		if (!node.isNull())
		{
			return YamlToolPrivate::getInstance()->_getDef(node.m_pImplNode->node(), key, def);
		}
		else
		{
			return def;
		}
	}
	void YamlTool::_set(YamlNode& node, const std::string& key, const std::any& value)
	{
		YamlToolPrivate::getInstance()->_set(node.m_pImplNode->node(), key, value);
	}

	bool YamlTool::_setDef(YamlNode& node, const std::string& key, const std::any& value)
	{
		return YamlToolPrivate::getInstance()->_setDef(node.m_pImplNode->node(), key, value);
	}
}// namespace Config