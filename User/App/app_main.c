/**
 * @file    app_main.c
 * @brief   系统应用入口，负责执行自动初始化表。
 * @author  Ltttttts
 *
 * 初始化顺序由 auto_init_defs.h 中的 level 决定：
 *   Level 1 (BOARD):   BSP_LED_Init, BSP_SD_Init
 *   Level 2 (PREV):    Logger_Init
 *   Level 3 (DEVICE):  BSP_LCD_Construct
 *   Level 6 (APP):     各 RTOS 任务创建
 */

#include "cmsis_os.h"
#include "logger.h"
#include "auto_init.h"

#include "bsp_led.h"
#include "bsp_lcd.h"
#include "bsp_key.h"
#include "bsp_sd.h"

#include "task_led.h"
#include "task_lvgl.h"
#include "task_key.h"
#include "task_imu.h"
#include "task_uart.h"

/* 任务栈大小 */
#define TASK_STACK_SMALL     (256U * 4U)
#define TASK_STACK_LARGE     (2048U * 4U)

/* 任务句柄，供其他模块按需 extern 引用。 */
osThreadId_t ledTaskHandle;
osThreadId_t flowLedTaskHandle;
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

static const osThreadAttr_t s_flow_led_task_attr = {
    .name = "FlowLED_Task",
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

/* ========== 任务创建包装函数，属于自动初始化 Level 6 ========== */

void prv_create_led_task(void)
{
    ledTaskHandle = osThreadNew(Task_LED_Entry, NULL,
                                &s_led_task_attr);
    flowLedTaskHandle = osThreadNew(Task_FlowLED_Entry, NULL,
                                    &s_flow_led_task_attr);
}

void prv_create_lvgl_task(void)
{
    lvglTaskHandle = osThreadNew(
        Task_LVGL_Entry, NULL, &s_lvgl_task_attr);
}

void prv_create_key_task(void)
{
    keyTaskHandle = osThreadNew(Task_Key_Entry, NULL,
                                &s_key_task_attr);
}

void prv_create_uart_task(void)
{
    uartTaskHandle = osThreadNew(
        Task_UART_Entry, NULL, &s_uart_task_attr);
}

void prv_create_imu_task(void)
{
    imuTaskHandle = osThreadNew(Task_IMU_Entry, NULL,
                                &s_imu_task_attr);
}

/* ========== 系统入口 ========== */

void StartTask_Entry(void *argument)
{
    (void)argument;

    taskENTER_CRITICAL();

    LOG_I("SYS", "RTOS Kernel Started.");
    auto_init_run();

    taskEXIT_CRITICAL();

    LOG_I("SYS",
          "System initialization complete. StartTask self-deleting.");
    osThreadExit();
}
