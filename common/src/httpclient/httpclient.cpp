/*************************************************
  * 描述：HTTPS 客户端 — 公有接口 → 私有实现 转发层（PIMPL）
  *
  * File：httpclient.cpp
  * Author：chenyujin@mozihealthcare.cn
  * Date：2026/8/12
  * Update：
  * ************************************************/
#include <httpclient/httpclient.h>
#include <httpclient_p.h>

// ═══════════════════════════════════════════════════════════════
//  GET
// ═══════════════════════════════════════════════════════════════

HttpResponse HttpsClient::get(const std::string& url,
                              const std::map<std::string, std::string>& headers)
{
	return HttpsClientPrivate::get(url, headers);
}

// ═══════════════════════════════════════════════════════════════
//  POST
// ═══════════════════════════════════════════════════════════════

HttpResponse HttpsClient::postJson(const std::string& url,
                                   const std::string& jsonBody,
                                   const std::map<std::string, std::string>& extraHeaders)
{
	return HttpsClientPrivate::postJson(url, jsonBody, extraHeaders);
}

HttpResponse HttpsClient::postForm(const std::string& url,
                                   const std::map<std::string, std::string>& formData,
                                   const std::map<std::string, std::string>& extraHeaders)
{
	return HttpsClientPrivate::postForm(url, formData, extraHeaders);
}

// ═══════════════════════════════════════════════════════════════
//  全局配置
// ═══════════════════════════════════════════════════════════════

void HttpsClient::setVerifySsl(bool verify)
{
	HttpsClientPrivate::setVerifySsl(verify);
}

void HttpsClient::setCaBundlePath(const std::string& caPath)
{
	HttpsClientPrivate::setCaBundlePath(caPath);
}

void HttpsClient::setTimeout(long timeoutSecs)
{
	HttpsClientPrivate::setTimeout(timeoutSecs);
}

void HttpsClient::setConnectTimeout(long timeoutSecs)
{
	HttpsClientPrivate::setConnectTimeout(timeoutSecs);
}
