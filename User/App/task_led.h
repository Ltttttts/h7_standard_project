#ifndef __TASK_LED_H
#define __TASK_LED_H

#include "cmsis_os2.h"
#include <stdint.h>

/* 状态灯工作模式 */
typedef enum {
    LED_MODE_ON = 0,
    LED_MODE_OFF,
    LED_MODE_BLINK
} LedMode_e;

/* 流水灯工作模式 */
typedef enum {
    FLOW_LED_MODE_OFF = 0,
    FLOW_LED_MODE_FORWARD,
    FLOW_LED_MODE_REVERSE,
    FLOW_LED_MODE_BOUNCE,
    FLOW_LED_MODE_ALL_BLINK
} FlowLedMode_e;

/* UI 发送给状态灯任务的控制消息 */
typedef struct {
    LedMode_e mode;
    uint32_t  interval_ms; /* 仅在闪烁模式下使用 */
} LedCmdMsg_t;

/* UI 发送给流水灯任务的控制消息 */
typedef struct {
    FlowLedMode_e mode;
    uint32_t      interval_ms;
} FlowLedCmdMsg_t;

/* 对外暴露消息队列，供 UI 页面发送控制命令 */
extern osMessageQueueId_t led_cmd_queue;
extern osMessageQueueId_t flow_led_cmd_queue;

void Task_LED_Entry(void *argument);
void Task_FlowLED_Entry(void *argument);

#endif /* __TASK_LED_H */


