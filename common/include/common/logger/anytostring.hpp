/*************************************************
  * 描述：
  *
  * File：anytostring.h
  * Date：2026/2/11
  * Update：2026/7/13 — 优化类型分发 O(1)，替换废弃 codecvt
  * ************************************************/
#ifndef LOGGER_ANYTOSTRING_H
#define LOGGER_ANYTOSTRING_H
#include <any>
#include <charconv>
#include <functional>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <codecvt>
#include <locale>
#endif

namespace LoggerUtil
{
    inline std::string floatingNumToString(double value)
    {
        if (value == 0.0 || value == -0.0)
            return "0";

        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
        if (ec != std::errc{})
            return std::to_string(value);  // fallback
        return std::string(buf, ptr);
    }

    /// 宽字符转 UTF-8（不使用已废弃的 std::codecvt）
    inline std::string wstringToUtf8(std::wstring_view wstr)
    {
        if (wstr.empty()) return {};
#ifdef _WIN32
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                      nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string result(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                            result.data(), len, nullptr, nullptr);
        return result;
#else
        // POSIX：C++17 标记废弃但 C++26 前仍可用
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        auto result = converter.to_bytes(wstr.data());
        #pragma GCC diagnostic pop
        return result;
#endif
    }

    /// O(1) 类型分发表（静态初始化一次）
    using AnyConverter = std::function<std::optional<std::string>(const std::any&)>;

    inline const std::unordered_map<std::type_index, AnyConverter>& getConverters()
    {
        static const std::unordered_map<std::type_index, AnyConverter> map = {
            {std::type_index(typeid(std::string)),
             [](const std::any& a) -> std::optional<std::string> { return std::any_cast<std::string>(a); }},
            {std::type_index(typeid(const char*)),
             [](const std::any& a) -> std::optional<std::string> { return std::any_cast<const char*>(a); }},
            {std::type_index(typeid(char*)),
             [](const std::any& a) -> std::optional<std::string> { return std::any_cast<char*>(a); }},
            {std::type_index(typeid(int)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<int>(a)); }},
            {std::type_index(typeid(unsigned int)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<unsigned int>(a)); }},
            {std::type_index(typeid(long)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<long>(a)); }},
            {std::type_index(typeid(long long)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<long long>(a)); }},
            {std::type_index(typeid(float)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<float>(a)); }},
            {std::type_index(typeid(double)),
             [](const std::any& a) -> std::optional<std::string> { return floatingNumToString(std::any_cast<double>(a)); }},
            {std::type_index(typeid(size_t)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<size_t>(a)); }},
            {std::type_index(typeid(uint64_t)),
             [](const std::any& a) -> std::optional<std::string> { return std::to_string(std::any_cast<uint64_t>(a)); }},
            {std::type_index(typeid(bool)),
             [](const std::any& a) -> std::optional<std::string> { return std::any_cast<bool>(a) ? std::string("true") : std::string("false"); }},
            {std::type_index(typeid(std::wstring)),
             [](const std::any& a) -> std::optional<std::string> { return wstringToUtf8(std::any_cast<std::wstring>(a)); }},
            {std::type_index(typeid(wchar_t*)),
             [](const std::any& a) -> std::optional<std::string> { return wstringToUtf8(std::any_cast<wchar_t*>(a)); }},
            {std::type_index(typeid(const wchar_t*)),
             [](const std::any& a) -> std::optional<std::string> { return wstringToUtf8(std::any_cast<const wchar_t*>(a)); }},
        };
        return map;
    }

    inline std::optional<std::string> anyToString(const char* /*fileName*/, int /*fileLine*/,
                                                  const char* /*function*/, const std::any& data)
    {
        if (!data.has_value())
            return std::nullopt;
        if (data.type() == typeid(nullptr))
            return std::nullopt;

        auto& converters = getConverters();
        auto it = converters.find(std::type_index(data.type()));
        if (it != converters.end())
            return it->second(data);

        return std::nullopt;
    }
}

#endif //LOGGER_ANYTOSTRING_H
