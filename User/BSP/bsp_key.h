#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include <stdint.h>

/* 按键事件枚举 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_CLICK,       // 短按
    KEY_EVENT_LONG_PRESS   // 长按
} Key_Event_e;

/* 面向对象：按键“类”的定义 */
typedef struct Key_t {
    uint8_t  id;                    // 按键 ID，方便调试或区分
    uint8_t  (*ReadPin_CB)(uint8_t id);   // 【虚函数】底层硬件读取回调接口 (返回1表示按下，0表示松开)
    
    uint8_t  state;                 // 内部状态机当前状态
    uint16_t tick_cnt;              // 内部计时器 (用于消抖和长按计时)
    uint16_t long_press_thresh;     // 长按触发阈值 (毫秒)
	
    
    Key_Event_e event;              // 对外输出的按键事件
} Key_t;

/* 类的“成员方法” */
void Key_Init(Key_t *key, uint8_t id, uint8_t (*ReadPinFunc)(uint8_t), uint16_t lp_thresh_ms);
void Key_Update(Key_t *key, uint8_t tick_ms); // 状态机心跳 (放在定时器或任务中)
Key_Event_e Key_GetEvent(Key_t *key);         // 获取并清除事件

#endif



