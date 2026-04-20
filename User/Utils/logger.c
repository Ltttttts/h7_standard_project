#include "logger.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* 引入你的串口句柄 (假设配置在 usart.c 中) */
extern UART_HandleTypeDef huart1; 

static SemaphoreHandle_t xLoggerMutex = NULL;

#define LOG_BUFFER_SIZE 256
static char log_buf[LOG_BUFFER_SIZE];

void Logger_Init(void) {
    // 创建互斥锁
    xLoggerMutex = xSemaphoreCreateMutex();
}

void Logger_Print(LogLevel_t level, const char *tag, const char *fmt, ...) {
    uint32_t tick = 0;
    
    // 获取时间戳：判断 FreeRTOS 是否已经启动
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
    } else {
        tick = HAL_GetTick();
    }

    // 上锁：保护全局的 log_buf 和底层串口
    if (xLoggerMutex != NULL) {
        xSemaphoreTake(xLoggerMutex, portMAX_DELAY);
    }

    // 1. 写入时间戳
    int offset = snprintf(log_buf, LOG_BUFFER_SIZE, "[%8lu] ", tick);
    
    // 2. 写入用户日志内容
    if (offset < LOG_BUFFER_SIZE) {
        va_list args;
        va_start(args, fmt);
        // vsnprintf 会自动把多个参数组合成字符串，并防止数组越界
        vsnprintf(log_buf + offset, LOG_BUFFER_SIZE - offset, fmt, args);
        va_end(args);
    }

    // 3. 调用你的 HAL 库进行阻塞式打印
    uint16_t len = strlen(log_buf);
    HAL_UART_Transmit(&huart1, (uint8_t*)log_buf, len, HAL_MAX_DELAY);

    // 解锁
    if (xLoggerMutex != NULL) {
        xSemaphoreGive(xLoggerMutex);
    }
}

#pragma import(__use_no_semihosting)

//// 标准库需要的支持对象
//struct __FILE {
//    int handle;
//};
//FILE __stdout;

// 定义 _sys_exit 防止半主机模式报错
void _sys_exit(int x) {
    x = x;
}

// 真正重定向底层的 fputc 函数
int fputc(int ch, FILE *f) {
    // printf 是一字节一字节发的，这里直接调 HAL 库发送
    // 注意：不要在这里加 Mutex，否则高频 printf 时会死锁
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
    return ch;
}


