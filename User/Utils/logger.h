#ifndef __LOGGER_H
#define __LOGGER_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* 日志级别 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE
} LogLevel_t;

/* 全局日志级别控制 */
#define SYSTEM_LOG_LEVEL  LOG_LEVEL_DEBUG

void Logger_Init(void);
void Logger_Print(LogLevel_t level, const char *tag, const char *fmt, ...);

/* 无颜色的标准输出宏 */
#if (SYSTEM_LOG_LEVEL <= LOG_LEVEL_DEBUG)
    #define LOG_D(TAG, fmt, ...) Logger_Print(LOG_LEVEL_DEBUG, TAG, "[D/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#else
    #define LOG_D(TAG, fmt, ...)
#endif

#if (SYSTEM_LOG_LEVEL <= LOG_LEVEL_INFO)
    #define LOG_I(TAG, fmt, ...) Logger_Print(LOG_LEVEL_INFO,  TAG, "[I/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#else
    #define LOG_I(TAG, fmt, ...)
#endif

#define LOG_W(TAG, fmt, ...) Logger_Print(LOG_LEVEL_WARN,  TAG, "[W/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#define LOG_E(TAG, fmt, ...) Logger_Print(LOG_LEVEL_ERROR, TAG, "[E/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)

#endif /* __LOGGER_H */

