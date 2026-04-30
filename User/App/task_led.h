#ifndef __TASK_LED_H
#define __TASK_LED_H

#include "cmsis_os2.h"
#include <stdint.h>

// 定义 LED 的三种工作模式
typedef enum {
    LED_MODE_ON = 0,
    LED_MODE_OFF,
    LED_MODE_BLINK
} LedMode_e;

// 定义 UI 发送给 LED 任务的指令包
typedef struct {
    LedMode_e mode;
    uint32_t  interval_ms; // 仅在 BLINK 模式下有效
} LedCmdMsg_t;

// 暴露消息队列句柄供 UI 发送消息
extern osMessageQueueId_t led_cmd_queue;

void Task_LED_Entry(void *argument);

#endif // __TASK_LED_H


