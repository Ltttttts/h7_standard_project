#include "task_key.h"
#include "input_manager.h" // 引入标准头文件

// 实例化全局邮箱
osMessageQueueId_t input_msg_queue = NULL;

#define KEY_NUM 4
static Key_t Keys[KEY_NUM];


/* =======================================================
 * 抽象按键状态查询 (提供给 UI 轮询使用，隔绝硬件细节)
 * ======================================================= */
bool Input_Is_Key_Held(SysKey_e key_code) 
{
    // 这里将抽象的虚拟按键，反向映射回物理硬件状态
    if (key_code == SYS_KEY_UP) {
        return (Drv_Key_Read(1) == 1); 
    }
    if (key_code == SYS_KEY_DOWN) {
        return (Drv_Key_Read(2) == 1); 
    }
    // 【核心修复】：补齐确定键和返回键的状态查询！
    if (key_code == SYS_KEY_ENTER) {
        return (Drv_Key_Read(3) == 1); 
    }
    if (key_code == SYS_KEY_BACK) {
        return (Drv_Key_Read(4) == 1); 
    }
    
    // 串口键盘的“长按”由电脑自动连发实现，不需要在这里轮询状态
    return false; 
}


void Task_Key_Entry(void *argument) 
{
    // 创建邮箱，现在叫 input_msg_queue
    input_msg_queue = osMessageQueueNew(10, sizeof(InputEventMsg_t), NULL);
    
    for(int i = 0; i < KEY_NUM; i++) Key_Init(&Keys[i], i + 1, Drv_Key_Read, 1000);

    while (1) {
        for(int i = 0; i < KEY_NUM; i++) {
            Key_Update(&Keys[i], 10); 
            Key_Event_e event = Key_GetEvent(&Keys[i]);
            
            if (event != KEY_EVENT_NONE) {
                InputEventMsg_t msg = {0};
                msg.event = event;
                msg.source = INPUT_SRC_HW_KEY;
                
                // 【核心映射】：硬件按键 ID -> 虚拟按键码
                switch (Keys[i].id) {
                    case 1: msg.key_code = SYS_KEY_UP; break;
                    case 2: msg.key_code = SYS_KEY_DOWN; break;
                    case 3: msg.key_code = SYS_KEY_ENTER; break;
                    case 4: msg.key_code = SYS_KEY_BACK; break;
                    default: msg.key_code = SYS_KEY_NONE; break;
                }
                
                if (msg.key_code != SYS_KEY_NONE) {
                    osMessageQueuePut(input_msg_queue, &msg, 0, 0);
                }
            }
        }
        osDelay(10); 
    }
}


