#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "led_dev.h"
#include "main.h"

/* 板级流水灯编号。 */
typedef enum {
    BSP_LED_FLOW_1 = 0,
    BSP_LED_FLOW_2,
    BSP_LED_FLOW_3,
    BSP_LED_FLOW_4,
    BSP_LED_FLOW_NUM
} BSP_LED_FlowId_t;

/* CubeMX 生成 GPIO 后，在这里替换为实际端口和引脚宏。 */
#ifndef BSP_LED1_GPIO_Port
#define BSP_LED1_GPIO_Port  GPIOF
#define BSP_LED1_Pin        GPIO_PIN_0
#endif

#ifndef BSP_LED2_GPIO_Port
#define BSP_LED2_GPIO_Port  GPIOF
#define BSP_LED2_Pin        GPIO_PIN_2
#endif

#ifndef BSP_LED3_GPIO_Port
#define BSP_LED3_GPIO_Port  GPIOF
#define BSP_LED3_Pin        GPIO_PIN_3
#endif

#ifndef BSP_LED4_GPIO_Port
#define BSP_LED4_GPIO_Port  GPIOF
#define BSP_LED4_Pin        GPIO_PIN_5
#endif

/* 板级 LED 设备对象。 */
extern LED_Device_t LedStatus;
extern LED_Device_t LedFlow[BSP_LED_FLOW_NUM];

/* 初始化板级 LED。 */
void BSP_LED_Init(void);

/* 关闭全部流水灯。 */
void BSP_LED_Flow_AllOff(void);

#endif /* __BSP_LED_H */


