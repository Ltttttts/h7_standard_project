#include "task_lcd.h"
#include "bsp_lcd.h"   // 引入屏幕对象
#include "logger.h"
#include "cmsis_os2.h"

static const char* TAG = "Task_Disp";

void Task_Display_Entry(void *argument) {
    uint32_t count = 0;
    
    LOG_I(TAG, "Display task started.");

    /* 在系统的 StartTask 中，已经调用过 BSP_LCD_Construct() 
     * 把方法装载到了 Screen 对象上。
     */
    
    // 1. 初始化屏幕 (内部自动创建 Mutex 并处理底层)
    Screen.Init(&Screen);
    
    // 2. 静态 UI 设置
    Screen.SetColor(&Screen, LCD_WHITE, LCD_BLACK);
    Screen.Clear(&Screen);
    Screen.ShowStr(&Screen, 0, 0, "!#$'()*+,-.0123");
    Screen.SetBacklight(&Screen, 1); // 亮屏

    // 3. 动态刷新
    while(1)
    {
        count++;
        
        // 我们不需要再写 LCD_LOCK()，因为 ShowNum 内部自己会加锁！
        // 其他任务这时候如果也想打印屏幕，会被对象自己的锁挡住排队，极度安全。
        Screen.SetColor(&Screen, LCD_GREEN, LCD_BLACK);
        Screen.ShowStr(&Screen, 0, 30, "Uptime:");
        Screen.ShowNum(&Screen, 80, 30, count, 5);

        osDelay(1000);
    }
}



