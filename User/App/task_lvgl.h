#ifndef __TASK_LVGL_H
#define __TASK_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  LVGL 核心图形引擎任务入口
 * @param  argument: 任务创建时传入的参数 (通常为 NULL)
 * @retval 无
 */
void Task_LVGL_Entry(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_LVGL_H */


