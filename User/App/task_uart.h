#ifndef __TASK_UART_H
#define __TASK_UART_H

#include "cmsis_os2.h"

// 暴露一个给中断用的原始字节队列句柄
extern osMessageQueueId_t uart_rx_queue;

void Task_UART_Entry(void *argument);

#endif

