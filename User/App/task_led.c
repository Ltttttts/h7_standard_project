#include "task_led.h"
#include "bsp_led.h"
#include "logger.h"
#include "cmsis_os2.h"

static const char* TAG = "Task_LED";

/**
 * @brief LED 闪烁任务入口
 */
void Task_LED_Entry(void *argument) {
    LOG_I(TAG, "LED Task Started.");

    while (1) {
        LedStatus.Toggle(&LedStatus);
        
        osDelay(500); 
    }
}



