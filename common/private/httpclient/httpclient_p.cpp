/*************************************************
  * 描述：HTTPS 客户端私有实现
  *
  * File：httpclient_p.cpp
  * Author：chenyujin@mozihealthcare.cn
  * Date：2026/8/12
  * Update：
  * ************************************************/
#include "httpclient_p.h"

#include <logger/logger.h>

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <sstream>

// ═══════════════════════════════════════════════════════════════
//  静态成员初始化
// ═══════════════════════════════════════════════════════════════
std::atomic<bool> HttpsClientPrivate::s_verifySsl{true};
std::string       HttpsClientPrivate::s_caBundlePath;
std::atomic<long> HttpsClientPrivate::s_timeout{30};
std::atomic<long> HttpsClientPrivate::s_connectTimeout{10};

// ═══════════════════════════════════════════════════════════════
//  libcurl 回调
// ═══════════════════════════════════════════════════════════════

size_t HttpsClientPrivate::writeCallback(void* data, size_t size, size_t nmemb, void* userp)
{
	const size_t realSize = size * nmemb;
	auto*        body     = static_cast<std::string*>(userp);
	if (body)
	{
		body->append(static_cast<char*>(data), realSize);
	}
	return realSize;
}

size_t HttpsClientPrivate::headerCallback(void* data, size_t size, size_t nmemb, void* userp)
{
	const size_t realSize = size * nmemb;
	auto*        headers  = static_cast<std::map<std::string, std::string>*>(userp);
	if (!headers) return realSize;

	std::string line(static_cast<char*>(data), realSize);

	// 跳过 HTTP 状态行（如 "HTTP/1.1 200 OK\r\n"）
	if (line.find("HTTP/") == 0) return realSize;

	// 跳过空行
	if (line == "\r\n" || line == "\n") return realSize;

	// 去除尾部 \r\n
	while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
		line.pop_back();

	// 解析 "Key: Value"
	const auto colonPos = line.find(':');
	if (colonPos != std::string::npos)
	{
		std::string key = line.substr(0, colonPos);
		std::string val = line.substr(colonPos + 1);

		// 跳过开头的空白
		size_t valStart = 0;
		while (valStart < val.size() && (val[valStart] == ' ' || val[valStart] == '\t'))
			++valStart;
		val = val.substr(valStart);

		// key 转小写，便于查找
		std::transform(key.begin(), key.end(), key.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		(*headers)[key] = val;
	}
	return realSize;
}

// ═══════════════════════════════════════════════════════════════
//  配置应用
// ═══════════════════════════════════════════════════════════════

void HttpsClientPrivate::applyGlobalConfig(CURL* curl)
{
	const bool verify = s_verifySsl.load(std::memory_order_relaxed);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, verify ? 2L : 0L);

	// 如果指定了 CA bundle 路径则使用，否则由 libcurl 自行查找系统证书存储
	if (!s_caBundlePath.empty())
	{
		curl_easy_setopt(curl, CURLOPT_CAINFO, s_caBundlePath.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_TIMEOUT, s_timeout.load(std::memory_order_relaxed));
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, s_connectTimeout.load(std::memory_order_relaxed));
}

void HttpsClientPrivate::setHeaders(CURL* curl,
                                    const std::map<std::string, std::string>& headers,
                                    struct curl_slist** slist)
{
	for (const auto& [key, val] : headers)
	{
		std::string headerLine = key + ": " + val;
		*slist = curl_slist_append(*slist, headerLine.c_str());
	}
	if (*slist)
	{
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, *slist);
	}
}

// ═══════════════════════════════════════════════════════════════
//  请求执行
// ═══════════════════════════════════════════════════════════════

HttpResponse HttpsClientPrivate::perform(CURL* curl)
{
	HttpResponse response;
	std::string  bodyBuffer;
	std::map<std::string, std::string> headerBuffer;

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bodyBuffer);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerBuffer);

	// 跟随重定向（最多 5 次）
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

	// 应用全局配置（SSL、超时）
	applyGlobalConfig(curl);

	// 读取实际请求 URL
	char* effectiveUrl = nullptr;
	curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);
	const char* url = effectiveUrl ? effectiveUrl : "(unknown)";

	LOG_INFO("HTTPS 请求开始: ", url);

	const CURLcode res = curl_easy_perform(curl);

	if (res == CURLE_OK)
	{
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
		response.body    = std::move(bodyBuffer);
		response.headers = std::move(headerBuffer);

		LOG_INFO("HTTPS 响应: HTTP ", response.statusCode,
		         ", body=", response.body.size(), " bytes  [", url, "]");
	}
	else
	{
		response.error      = curl_easy_strerror(res);
		response.statusCode = 0;
		response.body       = std::move(bodyBuffer);

		LOG_ERROR("HTTPS 请求失败: ", response.error, "  [", url, "]");
	}

	return response;
}

// ═══════════════════════════════════════════════════════════════
//  GET
// ═══════════════════════════════════════════════════════════════

HttpResponse HttpsClientPrivate::get(const std::string& url,
                                     const std::map<std::string, std::string>& headers)
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		HttpResponse resp;
		resp.error = "curl_easy_init() 返回 NULL";
		LOG_ERROR("GET ", url, " — ", resp.error);
		return resp;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

	struct curl_slist* slist = nullptr;
	setHeaders(curl, headers, &slist);

	HttpResponse resp = perform(curl);

	if (slist) curl_slist_free_all(slist);
	curl_easy_cleanup(curl);
	return resp;
}

// ═══════════════════════════════════════════════════════════════
//  POST JSON
// ═══════════════════════════════════════════════════════════════

HttpResponse HttpsClientPrivate::postJson(const std::string& url,
                                          const std::string& jsonBody,
                                          const std::map<std::string, std::string>& extraHeaders)
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		HttpResponse resp;
		resp.error = "curl_easy_init() 返回 NULL";
		LOG_ERROR("POST JSON ", url, " — ", resp.error);
		return resp;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(jsonBody.size()));

	// 构建请求头：Content-Type: application/json + 额外头
	std::map<std::string, std::string> allHeaders = extraHeaders;
	allHeaders["Content-Type"] = "application/json";

	struct curl_slist* slist = nullptr;
	setHeaders(curl, allHeaders, &slist);

	HttpResponse resp = perform(curl);

	if (slist) curl_slist_free_all(slist);
	curl_easy_cleanup(curl);
	return resp;
}

// ═══════════════════════════════════════════════════════════════
//  POST Form
// ═══════════════════════════════════════════════════════════════

HttpResponse HttpsClientPrivate::postForm(const std::string& url,
                                          const std::map<std::string, std::string>& formData,
                                          const std::map<std::string, std::string>& extraHeaders)
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		HttpResponse resp;
		resp.error = "curl_easy_init() 返回 NULL";
		LOG_ERROR("POST Form ", url, " — ", resp.error);
		return resp;
	}

	// 拼接 application/x-www-form-urlencoded body
	std::ostringstream formBody;
	bool first = true;
	for (const auto& [key, val] : formData)
	{
		if (!first) formBody << '&';
		first = false;

		// 简易 URL 编码（只处理最常见的情况）
		char* escapedKey = curl_easy_escape(curl, key.c_str(), static_cast<int>(key.size()));
		char* escapedVal = curl_easy_escape(curl, val.c_str(), static_cast<int>(val.size()));
		if (escapedKey) { formBody << escapedKey; curl_free(escapedKey); }
		formBody << '=';
		if (escapedVal) { formBody << escapedVal; curl_free(escapedVal); }
	}
	std::string formBodyStr = formBody.str();

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, formBodyStr.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(formBodyStr.size()));

	// 构建请求头
	std::map<std::string, std::string> allHeaders = extraHeaders;
	allHeaders["Content-Type"] = "application/x-www-form-urlencoded";

	struct curl_slist* slist = nullptr;
	setHeaders(curl, allHeaders, &slist);

	HttpResponse resp = perform(curl);

	if (slist) curl_slist_free_all(slist);
	curl_easy_cleanup(curl);
	return resp;
}

// ═══════════════════════════════════════════════════════════════
//  全局配置
// ═══════════════════════════════════════════════════════════════

void HttpsClientPrivate::setVerifySsl(bool verify)
{
	s_verifySsl.store(verify, std::memory_order_relaxed);
	LOG_INFO("SSL 证书验证: ", verify ? "启用" : "禁用");
}

void HttpsClientPrivate::setCaBundlePath(const std::string& caPath)
{
	s_caBundlePath = caPath;
	LOG_INFO("CA bundle 路径已设置: ", caPath);
}

void HttpsClientPrivate::setTimeout(long timeoutSecs)
{
	s_timeout.store(timeoutSecs, std::memory_order_relaxed);
	LOG_INFO("请求总超时已设为 ", timeoutSecs, "s");
}

void HttpsClientPrivate::setConnectTimeout(long timeoutSecs)
{
	s_connectTimeout.store(timeoutSecs, std::memory_order_relaxed);
	LOG_INFO("连接超时已设为 ", timeoutSecs, "s");
}
