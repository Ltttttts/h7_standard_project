#ifndef __INPUT_MANAGER_H
#define __INPUT_MANAGER_H

#include "cmsis_os2.h"
#include "bsp_key.h" 
#include <stdbool.h> 

// 1. 定义系统级标准虚拟按键
typedef enum {
    SYS_KEY_NONE = 0,
    SYS_KEY_UP,       // 上移 (对应硬件Key1 / 串口 'W')
    SYS_KEY_DOWN,     // 下移 (对应硬件Key2 / 串口 'S')
    SYS_KEY_ENTER,    // 确认 (对应硬件Key3 / 串口 'Enter')
    SYS_KEY_BACK      // 返回 (对应硬件Key4 / 串口 'Esc' 或 'Q')
} SysKey_e;


#define INPUT_SRC_HW_KEY 0  // 来自物理按键
#define INPUT_SRC_UART   1  // 来自串口控制
// 2. 统一的输入消息结构体
typedef struct {
    SysKey_e key_code;   // 统一的虚拟按键码
    Key_Event_e event;   // 事件类型 (CLICK, PRESSED 等)
    uint8_t source;      // 【新增】：消息来源
} InputEventMsg_t;

#define INPUT_SRC_HW_KEY 0  // 来自物理按键
#define INPUT_SRC_UART   1  // 来自串口控制




// 3. 声明全局消息队列
extern osMessageQueueId_t input_msg_queue;

bool Input_Is_Key_Held(SysKey_e key_code);

#endif


