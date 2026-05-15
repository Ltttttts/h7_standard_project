#include "bsp_led.h"
#include "logger.h"

static const char* TAG = "BSP_LED";

/* 状态灯和 4 个流水灯的板级对象。 */
LED_Device_t LedStatus;
LED_Device_t LedFlow[BSP_LED_FLOW_NUM];

void BSP_LED_Init(void) 
{
    LED_Device_Init(&LedStatus, GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);    
    LED_Device_Init(&LedFlow[BSP_LED_FLOW_1], BSP_LED1_GPIO_Port, BSP_LED1_Pin, GPIO_PIN_RESET);
    LED_Device_Init(&LedFlow[BSP_LED_FLOW_2], BSP_LED2_GPIO_Port, BSP_LED2_Pin, GPIO_PIN_RESET);
    LED_Device_Init(&LedFlow[BSP_LED_FLOW_3], BSP_LED3_GPIO_Port, BSP_LED3_Pin, GPIO_PIN_RESET);
    LED_Device_Init(&LedFlow[BSP_LED_FLOW_4], BSP_LED4_GPIO_Port, BSP_LED4_Pin, GPIO_PIN_RESET);

    BSP_LED_Flow_AllOff();

    LOG_I(TAG, "Status LED and flow LEDs initialized.");
}

void BSP_LED_Flow_AllOff(void)
{
    for (uint8_t i = 0U; i < BSP_LED_FLOW_NUM; i++) {
        LedFlow[i].Off(&LedFlow[i]);
    }
}
