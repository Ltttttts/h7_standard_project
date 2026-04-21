#include "cmsis_os.h"
#include "logger.h"

#include "bsp_led.h"
#include "bsp_lcd.h"
#include "bsp_key.h"
#include "bsp_sd.h"

#include "task_led.h"
#include "task_lcd.h"
#include "task_lvgl.h"
#include "task_key.h"
#include "task_imu.h"

osThreadId_t ledTaskHandle;
osThreadId_t lcdTaskHandle;
osThreadId_t lvglTaskHandle;
osThreadId_t keyTaskHandle;
osThreadId_t imuTaskHandle;


// 将 LED 任务的配置提取出来，方便管理
const osThreadAttr_t ledTask_attributes = {
    .name = "LED_Task",
    .stack_size = 256 * 4,
    .priority = (osPriority_t) osPriorityNormal, 
};


const osThreadAttr_t dispTask_attr = {
     .name = "DispTask",
     .stack_size = 1024 * 4, 
     .priority = (osPriority_t) osPriorityNormal,
};


const osThreadAttr_t lvglTask_attr = {
        .name = "LVGL_Task",
        .stack_size = 2048 * 4, 
        .priority = (osPriority_t) osPriorityNormal,
};

const osThreadAttr_t keyTask_attr = {
        .name = "KEY_Task",
        .stack_size = 256 * 4, 
        .priority = (osPriority_t) osPriorityNormal,
};


const osThreadAttr_t imuTask_attr = {
        .name = "IMU_Task",
        .stack_size = 256 * 4, 
        .priority = (osPriority_t) osPriorityNormal,
};




void StartTask_Entry(void *argument) {
    // 1. 进入临界区：防止初始化过程中被更高优先级的任务打断
    taskENTER_CRITICAL();

    // 2. 初始化核心系统组件
    Logger_Init();
    LOG_I("SYS", "RTOS Kernel Started.");

    // 3. 初始化硬件驱动 (BSP)
    BSP_LED_Init();
	BSP_LCD_Construct();
	uint8_t res = BSP_SD_Init();
	LOG_I("SYS","%d",res);
	

    // 4. 创建业务任务
    ledTaskHandle = osThreadNew(Task_LED_Entry, NULL, &ledTask_attributes);
    
    if (ledTaskHandle == NULL) {
        LOG_E("SYS", "Failed to create LED Task!");
    } else {
        LOG_I("SYS", "LED Task created successfully.");
    }
	
//	lcdTaskHandle = osThreadNew(Task_Display_Entry, NULL, &dispTask_attr);	
//    if (lcdTaskHandle == NULL) {
//        LOG_E("SYS", "Failed to create LCD Task!");
//    } else {
//        LOG_I("SYS", "LCD Task created successfully.");
//    }
	
	lvglTaskHandle = osThreadNew(Task_LVGL_Entry, NULL, &lvglTask_attr);
    if (lvglTaskHandle == NULL) {
        LOG_E("SYS", "Failed to create LVGL Task!");
    } else {
        LOG_I("SYS", "LVGL Task created successfully.");
    }	
	
	
	keyTaskHandle = osThreadNew(Task_Key_Entry, NULL, &keyTask_attr);
    if (keyTaskHandle == NULL) {
        LOG_E("SYS", "Failed to create key Task!");
    } else {
        LOG_I("SYS", "key Task created successfully.");
    }	
	
	
	imuTaskHandle = osThreadNew(Task_IMU_Entry, NULL, &imuTask_attr);
    if (imuTaskHandle == NULL) {
        LOG_E("SYS", "Failed to create imu Task!");
    } else {
        LOG_I("SYS", "imu Task created successfully.");
    }

	
    // 这里可以继续创建其他任务，如通信任务、传感器任务...

    // 5. 退出临界区
    taskEXIT_CRITICAL();

    // 6. 功成身退
    LOG_I("SYS", "System initialization complete. StartTask self-deleting.");
    osThreadExit(); 
}



