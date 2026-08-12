/*************************************************
  * 描述：HTTPS 客户端封装
  *       基于 libcurl + OpenSSL，提供 GET / POST 同步请求能力。
  *       全部静态方法，无需实例化，与 Logger 调用风格一致。
  *
  * File：httpclient.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2026/8/12
  * Update：
  * ************************************************/
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "export.h"
#include <map>
#include <string>

/**
 * @brief HTTP 响应结构体
 *
 * 包含状态码、响应体、响应头以及错误信息。
 * 通过 ok() 可快速判断请求是否成功（状态码 2xx 且无底层错误）。
 */
struct HTTP_CLIENT_API HttpResponse
{
	long statusCode = 0;                                ///< HTTP 状态码（0 表示未收到有效响应）
	std::string body;                                   ///< 响应体文本
	std::map<std::string, std::string> headers;         ///< 响应头（key 已转为小写）
	std::string error;                                  ///< 错误描述（成功时为空）

	/**
	 * @brief 请求是否成功
	 * @return true 当状态码在 [200, 300) 且 error 为空时
	 */
	[[nodiscard]] bool ok() const
	{
		return statusCode >= 200 && statusCode < 300 && error.empty();
	}
};

/**
 * @brief HTTPS 客户端
 *
 * 封装 libcurl 的常用 HTTPS GET / POST 操作。
 *
 * ## 快速示例
 * @code
 *   // GET 请求
 *   auto res = HttpsClient::get("https://api.example.com/data");
 *   if (res.ok()) { std::cout << res.body; }
 *
 *   // POST JSON
 *   auto res2 = HttpsClient::postJson("https://api.example.com/submit",
 *                                     R"({"name":"value"})",
 *                                     {{"Authorization","Bearer token123"}});
 * @endcode
 *
 * ## 前提条件
 * 1. OpenSSL 运行时 DLL 需在可执行文件同目录（CMake 已自动拷贝）。
 * 2. Windows 上 libcurl 不自带根证书，需指定 CA bundle 路径或关闭验证（仅调试用）：
 *    - HttpsClient::setCaBundlePath("cacert.pem")
 *    - HttpsClient::setVerifySsl(false)  // 仅测试！
 *
 * 详细说明见 doc/https_client_guide.md。
 */
class HTTP_CLIENT_API HttpsClient
{
public:
	// ═══════════════════════════════════════════════════════════
	//  GET
	// ═══════════════════════════════════════════════════════════

	/**
	 * @brief 发起 HTTPS GET 请求
	 * @param url     请求 URL（须以 https:// 开头，http:// 也兼容）
	 * @param headers 额外的请求头（如 {{"Authorization","Bearer xxx"}}）
	 * @return HttpResponse，通过 ok() 判断是否成功
	 */
	static HttpResponse get(const std::string& url,
	                        const std::map<std::string, std::string>& headers = {});

	// ═══════════════════════════════════════════════════════════
	//  POST
	// ═══════════════════════════════════════════════════════════

	/**
	 * @brief 发起 HTTPS POST 请求，请求体为 JSON
	 * @param url          请求 URL
	 * @param jsonBody     JSON 字符串（不会自动转义，请传入合法 JSON）
	 * @param extraHeaders 额外请求头（会自动追加 Content-Type: application/json）
	 * @return HttpResponse
	 */
	static HttpResponse postJson(const std::string& url,
	                             const std::string& jsonBody,
	                             const std::map<std::string, std::string>& extraHeaders = {});

	/**
	 * @brief 发起 HTTPS POST 请求，请求体为表单数据（application/x-www-form-urlencoded）
	 * @param url          请求 URL
	 * @param formData     表单键值对
	 * @param extraHeaders 额外请求头
	 * @return HttpResponse
	 */
	static HttpResponse postForm(const std::string& url,
	                             const std::map<std::string, std::string>& formData,
	                             const std::map<std::string, std::string>& extraHeaders = {});

	// ═══════════════════════════════════════════════════════════
	//  全局配置（线程安全）
	// ═══════════════════════════════════════════════════════════

	/**
	 * @brief 启用/禁用 SSL 证书验证
	 *
	 * 默认启用。仅在调试自签证书的内网环境时可设为 false。
	 * **切勿在生产环境关闭验证。**
	 * @param verify true=验证（默认），false=跳过验证
	 */
	static void setVerifySsl(bool verify);

	/**
	 * @brief 指定自定义 CA 证书包路径
	 *
	 * Windows 上 libcurl 不自带根证书存储，调用此方法指定 cacert.pem 路径。
	 * @param caPath CA bundle 文件的绝对或相对路径（如 "cacert.pem"）
	 * @note 必需文件格式：PEM（Privacy Enhanced Mail）
	 * @note 可从 https://curl.se/ca/cacert.pem 下载
	 */
	static void setCaBundlePath(const std::string& caPath);

	/**
	 * @brief 设置请求总超时时间（秒）
	 * @param timeoutSecs 超时秒数，默认 30
	 */
	static void setTimeout(long timeoutSecs);

	/**
	 * @brief 设置连接建立超时时间（秒）
	 * @param timeoutSecs 超时秒数，默认 10
	 */
	static void setConnectTimeout(long timeoutSecs);

private:
	HttpsClient() = delete;
};

#endif //HTTP_CLIENT_H
