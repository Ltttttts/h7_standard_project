#ifndef __LOGGER_H
#define __LOGGER_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

/* 是否启用日志输出：1=启用，0=禁用（在禁用时所有 LOG_* 宏为无操作） */
#ifndef LOGGER_ENABLE
#define LOGGER_ENABLE 1
#endif

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

/* 当 LOGGER_ENABLE==1 时，使用外部实现；当为 0 时，提供静态 inline 的无操作实现，避免链接对 logger.c 的依赖 */
#if LOGGER_ENABLE
void Logger_Init(void);
void Logger_Print(LogLevel_t level, const char *tag, const char *fmt, ...);
#else
static inline void Logger_Init(void) { (void)0; }
static inline void Logger_Print(LogLevel_t level, const char *tag, const char *fmt, ...) { (void)level; (void)tag; (void)fmt; }
#endif

/* 无颜色的标准输出宏，支持整体开关 LOGGER_ENABLE 和按级别编译去除 */
#if LOGGER_ENABLE

#if (SYSTEM_LOG_LEVEL <= LOG_LEVEL_DEBUG)
    #define LOG_D(TAG, fmt, ...) Logger_Print(LOG_LEVEL_DEBUG, TAG, "[D/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#else
    #define LOG_D(TAG, fmt, ...) ((void)0)
#endif

#if (SYSTEM_LOG_LEVEL <= LOG_LEVEL_INFO)
    #define LOG_I(TAG, fmt, ...) Logger_Print(LOG_LEVEL_INFO,  TAG, "[I/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#else
    #define LOG_I(TAG, fmt, ...) ((void)0)
#endif

#if (SYSTEM_LOG_LEVEL <= LOG_LEVEL_WARN)
    #define LOG_W(TAG, fmt, ...) Logger_Print(LOG_LEVEL_WARN,  TAG, "[W/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#else
    #define LOG_W(TAG, fmt, ...) ((void)0)
#endif

#if (SYSTEM_LOG_LEVEL <= LOG_LEVEL_ERROR)
    #define LOG_E(TAG, fmt, ...) Logger_Print(LOG_LEVEL_ERROR, TAG, "[E/%s] " fmt "\r\n", TAG, ##__VA_ARGS__)
#else
    #define LOG_E(TAG, fmt, ...) ((void)0)
#endif

#else /* LOGGER_ENABLE == 0 */
    #define LOG_D(TAG, fmt, ...) ((void)0)
    #define LOG_I(TAG, fmt, ...) ((void)0)
    #define LOG_W(TAG, fmt, ...) ((void)0)
    #define LOG_E(TAG, fmt, ...) ((void)0)
#endif /* LOGGER_ENABLE */

#endif /* __LOGGER_H */

