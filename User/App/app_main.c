/**
 * @file    app_main.c
 * @brief   System main entry. Creates all RTOS tasks.
 * @author  Ltttttts
 */

#include "cmsis_os.h"
#include "logger.h"

#include "bsp_led.h"
#include "bsp_lcd.h"
#include "bsp_key.h"
#include "bsp_sd.h"

#include "task_led.h"
#include "task_lvgl.h"
#include "task_key.h"
#include "task_imu.h"
#include "task_uart.h"

/* 任务栈大小定义 */
#define TASK_STACK_SMALL     (256U * 4U)
#define TASK_STACK_MEDIUM    (1024U * 4U)
#define TASK_STACK_LARGE     (2048U * 4U)

/* 任务句柄定义 */
osThreadId_t ledTaskHandle;
osThreadId_t lcdTaskHandle;
osThreadId_t lvglTaskHandle;
osThreadId_t keyTaskHandle;
osThreadId_t imuTaskHandle;
osThreadId_t uartTaskHandle;

static const osThreadAttr_t s_led_task_attr = {
    .name = "LED_Task",
    .stack_size = TASK_STACK_SMALL,
    .priority = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t s_lvgl_task_attr = {
    .name = "LVGL_Task",
    .stack_size = TASK_STACK_LARGE,
    .priority = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t s_key_task_attr = {
    .name = "KEY_Task",
    .stack_size = TASK_STACK_SMALL,
    .priority = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t s_uart_task_attr = {
    .name = "UART_Task",
    .stack_size = TASK_STACK_SMALL,
    .priority = (osPriority_t)osPriorityNormal,
};

static const osThreadAttr_t s_imu_task_attr = {
    .name = "IMU_Task",
    .stack_size = TASK_STACK_SMALL,
    .priority = (osPriority_t)osPriorityNormal,
};

static void prv_create_task(const char *name,
                            osThreadFunc_t func,
                            const osThreadAttr_t *attr,
                            osThreadId_t *handle)
{
    *handle = osThreadNew(func, NULL, attr);
    if (*handle == NULL) {
        LOG_E("SYS", "Failed to create %s Task!", name);
    } else {
        LOG_I("SYS", "%s Task created successfully.", name);
    }
}

void StartTask_Entry(void *argument)
{
    taskENTER_CRITICAL();

    Logger_Init();
    LOG_I("SYS", "RTOS Kernel Started.");

    BSP_LED_Init();
    BSP_LCD_Construct();
    BSP_SD_Init();

    prv_create_task("LED", Task_LED_Entry,
                    &s_led_task_attr, &ledTaskHandle);

    prv_create_task("LVGL", Task_LVGL_Entry,
                    &s_lvgl_task_attr, &lvglTaskHandle);

    prv_create_task("Key", Task_Key_Entry,
                    &s_key_task_attr, &keyTaskHandle);

    prv_create_task("UART", Task_UART_Entry,
                    &s_uart_task_attr, &uartTaskHandle);

    prv_create_task("IMU", Task_IMU_Entry,
                    &s_imu_task_attr, &imuTaskHandle);

    taskEXIT_CRITICAL();

    LOG_I("SYS", "System initialization complete. StartTask self-deleting.");
    osThreadExit();
}



