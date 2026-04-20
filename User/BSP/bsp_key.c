#include "bsp_key.h"


// 内部状态机定义
#define STATE_IDLE           0
#define STATE_DEBOUNCE       1
#define STATE_WAIT_RELEASE   2
#define STATE_LONG_PRESSED   3

#define DEBOUNCE_TIME_MS     20  // 软件消抖时间：20ms

/**
 * @brief 按键对象“构造函数”
 */
void Key_Init(Key_t *key, uint8_t id, uint8_t (*ReadPinFunc)(uint8_t), uint16_t lp_thresh_ms)
{
    key->id = id;
    key->ReadPin_CB = ReadPinFunc;
    key->long_press_thresh = lp_thresh_ms;
    
    key->state = STATE_IDLE;
    key->tick_cnt = 0;
    key->event = KEY_EVENT_NONE;
}

/**
 * @brief 按键状态机更新 (需要在定时器或 RTOS 任务中周期调用)
 * @param tick_ms 距离上次调用经过了多少毫秒 (比如放 10ms 任务里，就传 10)
 */
void Key_Update(Key_t *key, uint8_t tick_ms) 
{
    // 1. 调用底层接口读取物理电平 (屏蔽了引脚的高低电平差异)
    uint8_t is_pressed = key->ReadPin_CB(key->id);

    // 2. 核心状态机
    switch (key->state) 
    {
        case STATE_IDLE:
            if (is_pressed) {
                key->state = STATE_DEBOUNCE;
                key->tick_cnt = 0; // 开始消抖计时
            }
            break;

        case STATE_DEBOUNCE:
            if (is_pressed) {
                key->tick_cnt += tick_ms;
                if (key->tick_cnt >= DEBOUNCE_TIME_MS) {
                    key->state = STATE_WAIT_RELEASE; // 消抖完成，确认按下
                    key->tick_cnt = 0; // 重置计时，准备算长按
                }
            } else {
                key->state = STATE_IDLE; // 抖动，回到初始状态
            }
            break;

        case STATE_WAIT_RELEASE:
            if (!is_pressed) {
                // 如果没达到长按时间就松开了，那就是短按 (Click)
                if (key->tick_cnt < key->long_press_thresh) {
                    key->event = KEY_EVENT_CLICK;
                }
                key->state = STATE_IDLE;
            } else {
                // 一直按着没松开，继续计时
                key->tick_cnt += tick_ms;
                if (key->tick_cnt >= key->long_press_thresh) {
                    key->event = KEY_EVENT_LONG_PRESS; // 触发长按
                    key->state = STATE_LONG_PRESSED;   // 进入等待松开状态(防止长按连发)
                }
            }
            break;

        case STATE_LONG_PRESSED:
            // 长按触发后，必须等完全松开手才允许下一次按键
            if (!is_pressed) {
                key->state = STATE_IDLE;
            }
            break;
    }
}

/**
 * @brief 读取并清除事件 (对外接口)
 */
Key_Event_e Key_GetEvent(Key_t *key) 
{
    Key_Event_e current_event = key->event;
    key->event = KEY_EVENT_NONE; // 读完就清零，防止重复处理
    return current_event;
}




