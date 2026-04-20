#ifndef __TASK_LED_H
#define __TASK_LED_H

/* * 按照“最小包含”原则：
 * 这里不需要包含 FreeRTOS.h，因为函数声明只用到了 void* 指针。
 * 保持头文件整洁可以加快编译速度。
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  LED 闪烁任务入口函数
 * @param  argument: 任务创建时传入的参数 (通常为 NULL)
 * @retval 无
 */
void Task_LED_Entry(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_LED_H */



