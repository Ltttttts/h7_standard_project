/**
 * @file    logger.c
 * @brief   Logging system with mutex protection.
 * @author  Ltttttts
 */

#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define LOG_BUFFER_SIZE     (256U)

extern UART_HandleTypeDef huart1;

#if LOGGER_ENABLE

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t s_logger_mutex = NULL;
static char s_log_buf[LOG_BUFFER_SIZE];

void Logger_Init(void)
{
    s_logger_mutex = xSemaphoreCreateMutex();
}
void Logger_Print(LogLevel_t level, const char *tag,
                  const char *fmt, ...)
{
    (void)level;
    uint32_t tick = 0;

    if (xTaskGetSchedulerState() !=
        taskSCHEDULER_NOT_STARTED) {
        tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
    } else {
        tick = HAL_GetTick();
    }

    if (s_logger_mutex != NULL) {
        xSemaphoreTake(s_logger_mutex, portMAX_DELAY);
    }

    int off = snprintf(s_log_buf, LOG_BUFFER_SIZE,
                       "[%8lu] ", tick);

    if (off < LOG_BUFFER_SIZE) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(s_log_buf + off,
                  LOG_BUFFER_SIZE - off, fmt, args);
        va_end(args);
    }

    uint16_t len = strlen(s_log_buf);
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)s_log_buf,
                      len, HAL_MAX_DELAY);

    if (s_logger_mutex != NULL) {
        xSemaphoreGive(s_logger_mutex);
    }
}

#endif /* LOGGER_ENABLE */

#pragma import(__use_no_semihosting)

void _sys_exit(int x)
{
    (void)x;
}

int fputc(int ch, FILE *f)
{
    (void)f;
    HAL_UART_Transmit(&huart1,
                      (uint8_t *)&ch, 1, 100);
    return ch;
}
