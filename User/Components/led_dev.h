#ifndef __LED_DEV_H
#define __LED_DEV_H

#include "stm32h7xx_hal.h"

/* LED 对象结构体 */
typedef struct LED_Device {
    GPIO_TypeDef* port;       // 绑定的 GPIO 端口
    uint16_t      pin;        // 绑定的引脚号
    GPIO_PinState active_lvl; // 有效电平
    
    /* 方法 (函数指针) */
    void (*On)(struct LED_Device* self);
    void (*Off)(struct LED_Device* self);
    void (*Toggle)(struct LED_Device* self);
} LED_Device_t;

/* 构造函数/初始化接口 */
void LED_Device_Init(LED_Device_t* dev, GPIO_TypeDef* port, uint16_t pin, GPIO_PinState active_lvl);

#endif /* __LED_DEV_H */



