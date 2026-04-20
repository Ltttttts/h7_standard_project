#include "led_dev.h"

/* 私有实现方法 */
static void LED_On_Impl(LED_Device_t* self) {
    HAL_GPIO_WritePin(self->port, self->pin, self->active_lvl);
}

static void LED_Off_Impl(LED_Device_t* self) {
    // 灭的电平与有效电平相反
    GPIO_PinState off_lvl = (self->active_lvl == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    HAL_GPIO_WritePin(self->port, self->pin, off_lvl);
}

static void LED_Toggle_Impl(LED_Device_t* self) {
    HAL_GPIO_TogglePin(self->port, self->pin);
}

/* 初始化函数：将方法绑定到对象上 */
void LED_Device_Init(LED_Device_t* dev, GPIO_TypeDef* port, uint16_t pin, GPIO_PinState active_lvl) {
    if (dev == NULL) return;
    
    dev->port = port;
    dev->pin = pin;
    dev->active_lvl = active_lvl;
    
    // 绑定函数指针
    dev->On = LED_On_Impl;
    dev->Off = LED_Off_Impl;
    dev->Toggle = LED_Toggle_Impl;
    
    // 默认初始化为熄灭状态
    dev->Off(dev);
}


