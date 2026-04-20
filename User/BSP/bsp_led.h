#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "led_dev.h"

/* 对外暴露系统状态 LED 对象 */
extern LED_Device_t LedStatus;

/* BSP LED 模块初始化 */
void BSP_LED_Init(void);

#endif /* __BSP_LED_H */


