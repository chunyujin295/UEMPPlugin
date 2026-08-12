/*************************************************
  * 描述：HTTP 客户端模块导出宏
  *
  * File：export.h
  * Author：chenyujin@mozihealthcare.cn
  * Date：2026/8/12
  * Update：
  * ************************************************/
#ifndef HTTP_CLIENT_EXPORT_H
#define HTTP_CLIENT_EXPORT_H

#if defined(_WIN32)
  #if defined(HTTP_CLIENT_BUILDING_LIBRARY)
    #define HTTP_CLIENT_API __declspec(dllexport)
  #else
    #define HTTP_CLIENT_API __declspec(dllimport)
  #endif
#else
  #define HTTP_CLIENT_API __attribute__((visibility("default")))
#endif

#endif //HTTP_CLIENT_EXPORT_H
