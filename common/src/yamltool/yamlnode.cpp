#include <yamltool/yamlnode.h>
#include <fstream>
#include <yamlnode_p.h>
namespace YamlTool
{
	YamlNode::YamlNode()
	{
		m_pImplNode = std::make_shared<YamlNodePrivate>();
	}
	YamlNode::YamlNode(const YamlNode& other)
	{
		m_pImplNode = std::make_shared<YamlNodePrivate>(other.m_pImplNode->m_node);
	}
	YamlNode& YamlNode::operator=(const YamlNode& other)
	{
		this->m_pImplNode = other.m_pImplNode;// 浅拷贝
		return *this;
	}
	YamlNode::YamlNode(const std::string& filePath)
	{
		m_pImplNode = std::make_shared<YamlNodePrivate>();
		// 文件存在性检查
		// if (!this->fileExists(filePath))
		// {
		//
		// 	throw std::runtime_error("file not exists: " + filePath);
		// 	return;
		// }
		// 读取和解析yaml文档
		try
		{
			m_pImplNode->m_node = YAML::LoadFile(filePath);// 包含文件存在性检查
		}
		catch (const YAML::BadFile& e)
		{
			// TODO 这里后面需要与小精灵面板进行结合，例如小此处抛出自定义异常，而精灵需要检测异常，然后弹出警告窗口
			throw std::runtime_error(e.what());
		}
		catch (const YAML::Exception& e)
		{
			throw std::runtime_error(e.what());
		}
	}
	YamlNode::~YamlNode() = default;

	bool YamlNode::isNull() const
	{
		return !m_pImplNode || m_pImplNode->m_node.IsNull();
	}
	bool YamlNode::isScalar() const
	{
		return m_pImplNode->m_node.IsScalar();
	}
	bool YamlNode::isMap() const
	{
		return m_pImplNode->m_node.IsMap();
	}
	bool YamlNode::isSequence() const
	{
		return m_pImplNode->m_node.IsSequence();
	}
	bool YamlNode::isDefined() const
	{
		return m_pImplNode->m_node.IsDefined();
	}
	// ConfigNode ConfigNode::operator[](const std::string& key) const
	// {
	// 	return ConfigNode(ConfigNodeImpl(m_pImplNode->m_node.operator[](key)));// 这里返回新的临时对象是可行的，因为底层YAML::Node是浅拷贝的
	// }
	// ConfigNode ConfigNode::operator[](size_t index) const
	// {
	// 	return ConfigNode(ConfigNodeImpl(m_pImplNode->m_node.operator[](index)));
	// }
	size_t YamlNode::size() const
	{
		return m_pImplNode->m_node.size();
	}
	YamlNode::YamlNode(const YamlNodePrivate& other)
	{
		m_pImplNode = std::make_shared<YamlNodePrivate>(other);
	}
	std::shared_ptr<YamlNode::YamlNodePrivate> YamlNode::pImplNode()
	{
		return m_pImplNode;
	}
	bool YamlNode::fileExists(const std::string& filePath)
	{
		const std::ifstream f(filePath.c_str());
		return f.good();
	}
}// namespace Config