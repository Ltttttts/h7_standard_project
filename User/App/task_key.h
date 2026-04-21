#ifndef __TASK_KEY_H
#define __TASK_KEY_H

#include "cmsis_os2.h"
#include "bsp_key.h"
#include "key_dev.h"

// 定义按键消息结构体
typedef struct {
    uint8_t id;          // 按键编号 (1, 2, 3, 4)
    Key_Event_e event;   // 按键事件 (CLICK, LONG_PRESS 等)
} KeyEventMsg_t;

// 暴露消息队列句柄，供 LVGL 任务读取
extern osMessageQueueId_t key_msg_queue;

// 暴露按键任务的启动/入口函数
void Task_Key_Entry(void *argument);

#endif

