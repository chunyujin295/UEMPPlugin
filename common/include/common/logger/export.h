#ifndef LOGGER_EXPORT_H
#define LOGGER_EXPORT_H

#if defined(_WIN32)
  #if defined(LOGGER_BUILDING_LIBRARY)
    #define LOGGER_API __declspec(dllexport)
  #else
    #define LOGGER_API __declspec(dllimport)
  #endif
#else
  #define LOGGER_API __attribute__((visibility("default")))
#endif


#endif //LOGGER_EXPORT_H