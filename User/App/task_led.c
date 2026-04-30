#include "task_led.h"
#include "bsp_led.h"   // 【核心修改 1】：引入你的面向对象 BSP 驱动

osMessageQueueId_t led_cmd_queue = NULL;

void Task_LED_Entry(void *argument) 
{
    // 如果你在 main.c 里没有调用初始化，请取消下面这行的注释：
    // BSP_LED_Init(); 

    // 1. 创建指令接收队列 (深度给10足够了)
    led_cmd_queue = osMessageQueueNew(10, sizeof(LedCmdMsg_t), NULL);
    
    // 2. 初始化默认状态：上电默认闪烁，间隔 500ms
    LedMode_e current_mode = LED_MODE_BLINK;
    uint32_t  blink_interval = 500;
    
    while(1) {
        LedCmdMsg_t cmd;
        
        // 如果当前是闪烁模式，等待时间就是闪烁间隔；如果是常亮/常灭，就死等指令 (osWaitForever)
        uint32_t wait_time = (current_mode == LED_MODE_BLINK) ? blink_interval : osWaitForever;
        
        // 尝试从队列获取新指令
        osStatus_t res = osMessageQueueGet(led_cmd_queue, &cmd, NULL, wait_time);
        
        // 如果收到了新指令，更新状态机
        if (res == osOK) {
            current_mode = cmd.mode;
            if (cmd.interval_ms >= 100) { // 限制最低 100ms，防止卡死
                blink_interval = cmd.interval_ms;
            }
        }
        
        // ==========================================================
        // 【核心修改 2】：使用你优雅的 OOP 接口控制硬件，告别 HAL 库
        // ==========================================================
        switch(current_mode) {
            case LED_MODE_ON:
                LedStatus.On(&LedStatus);     // 面向对象点亮
                break;
            case LED_MODE_OFF:
                LedStatus.Off(&LedStatus);    // 面向对象熄灭
                break;
            case LED_MODE_BLINK:
                LedStatus.Toggle(&LedStatus); // 面向对象翻转
                break;
        }
    }
}

