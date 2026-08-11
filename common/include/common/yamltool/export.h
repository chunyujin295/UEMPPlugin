/*************************************************
  * 描述：
  *
  * File：export.h
  * Author：3029795434@qq.com
  * Date：2025/12/10
  * Update：
  * ************************************************/
#ifndef YAML_TOOL_EXPORT_H
#define YAML_TOOL_EXPORT_H

#if defined(_WIN32)
  #if defined(YAML_TOOL_BUILDING_LIBRARY)
    #define YAML_TOOL_API __declspec(dllexport)
  #else
    #define YAML_TOOL_API __declspec(dllimport)
  #endif
#else
  #define YAML_TOOL_API __attribute__((visibility("default")))
#endif


#endif //YAML_TOOL_EXPORT_H