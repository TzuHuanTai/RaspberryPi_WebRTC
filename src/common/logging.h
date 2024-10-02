#ifndef LOGGING_H
#define LOGGING_H

#include <cstdio>
#include <string>

std::string GetFileName(const std::string &file_path);

#ifdef DEBUG_MODE
#define DEBUG_PRINT(fmt, ...) printf("[%s] " fmt "\n", GetFileName(__FILE__).c_str(), ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

#define ERROR_PRINT(fmt, ...)                                                                      \
    fprintf(stderr, "[%s] Error: " fmt "\n", GetFileName(__FILE__).c_str(), ##__VA_ARGS__)
#define INFO_PRINT(fmt, ...) printf("[%s] " fmt "\n", GetFileName(__FILE__).c_str(), ##__VA_ARGS__)

#endif // LOGGING_H
