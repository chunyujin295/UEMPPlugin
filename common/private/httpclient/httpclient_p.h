/*************************************************
  * 描述：HTTPS 客户端私有实现类
  *
  * File：httpclient_p.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2026/8/12
  * Update：
  * ************************************************/
#ifndef HTTP_CLIENT_P_H
#define HTTP_CLIENT_P_H

#include <httpclient/httpclient.h>
#include <curl/curl.h>

#include <atomic>
#include <string>

class HttpsClientPrivate
{
public:
	HttpsClientPrivate(const HttpsClientPrivate&) = delete;
	HttpsClientPrivate(HttpsClientPrivate&&) = delete;
	HttpsClientPrivate& operator=(const HttpsClientPrivate&) = delete;
	HttpsClientPrivate& operator=(HttpsClientPrivate&&) = delete;

	// ── 请求方法 ──
	static HttpResponse get(const std::string& url,
	                        const std::map<std::string, std::string>& headers);

	static HttpResponse postJson(const std::string& url,
	                             const std::string& jsonBody,
	                             const std::map<std::string, std::string>& extraHeaders);

	static HttpResponse postForm(const std::string& url,
	                             const std::map<std::string, std::string>& formData,
	                             const std::map<std::string, std::string>& extraHeaders);

	// ── 全局配置 ──
	static void setVerifySsl(bool verify);
	static void setCaBundlePath(const std::string& caPath);
	static void setTimeout(long timeoutSecs);
	static void setConnectTimeout(long timeoutSecs);

private:
	/**
	 * @brief 公共执行入口：配置 curl 句柄并同步执行请求
	 * @param curl 已调用 curl_easy_init() 的句柄
	 * @return HttpResponse
	 */
	static HttpResponse perform(CURL* curl);

	/**
	 * @brief 为 curl 句柄应用全局配置（SSL、超时等）
	 */
	static void applyGlobalConfig(CURL* curl);

	/**
	 * @brief 设置通用请求头
	 */
	static void setHeaders(CURL* curl,
	                       const std::map<std::string, std::string>& headers,
	                       struct curl_slist** slist);

	// libcurl 写回调（响应体）
	static size_t writeCallback(void* data, size_t size, size_t nmemb, void* userp);

	// libcurl 头回调（响应头）
	static size_t headerCallback(void* data, size_t size, size_t nmemb, void* userp);

	// ── 全局配置（线程安全） ──
	static std::atomic<bool> s_verifySsl;
	static std::string s_caBundlePath;          // 非 atomic：仅在初始化阶段设置一次
	static std::atomic<long> s_timeout;
	static std::atomic<long> s_connectTimeout;
};

#endif //HTTP_CLIENT_P_H
