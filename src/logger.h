#ifndef LOGGER_H
#define LOGGER_H
#include "config.h"

void LoggerBegin();
void LoggerTask(void *pv);

void logPrintf(const char *level, 
               const char *file,
               int line,
               const char *fmt,
               ...);

#define LOGI(fmt, ...) logPrintf("INFO", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) logPrintf("WARN", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) logPrintf("ERROR", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) logPrintf("DEBUG", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

void displayLogs();
void checkAndLogResetReason();
void clearLogHistory();

#endif