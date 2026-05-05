#include "bsp_led.h"
#include "logger.h"

static const char* TAG = "BSP_LED";

/* 实体化一个 LED 对象 */
LED_Device_t LedStatus;

void BSP_LED_Init(void) 
{
    LED_Device_Init(&LedStatus, GPIOG, GPIO_PIN_7, GPIO_PIN_RESET);    
    LOG_I(TAG, "Status LED on PG7 initialized.");
}

