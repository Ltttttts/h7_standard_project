#include "task_key.h"

osMessageQueueId_t key_msg_queue = NULL;

#define KEY_NUM 4
static Key_t Keys[KEY_NUM];

void Task_Key_Entry(void *argument) 
{
    // 1. 初始化消息队列 (容量设为 10，足以缓冲极快手速的连按)
    key_msg_queue = osMessageQueueNew(10, sizeof(KeyEventMsg_t), NULL);
    
    // 2. 初始化按键底层驱动
    for(int i = 0; i < KEY_NUM; i++) {
        Key_Init(&Keys[i], i + 1, Drv_Key_Read, 1000);
    }

    while (1) {
        // 3. 循环扫描按键
        for(int i = 0; i < KEY_NUM; i++) {
            Key_Update(&Keys[i], 10); // 假设任务周期是 10ms
            
            Key_Event_e event = Key_GetEvent(&Keys[i]);
            
            // 4. 如果有事件发生，发送到消息队列
            if (event != KEY_EVENT_NONE) {
                KeyEventMsg_t msg;
                msg.id = Keys[i].id;
                msg.event = event;
                
                // 将消息放入队列 (0表示如果队列满了不等待，直接丢弃)
                osMessageQueuePut(key_msg_queue, &msg, 0, 0);
            }
        }
        
        osDelay(10); // 释放 CPU 控制权，按键扫描 10ms 足够灵敏了
    }
}

