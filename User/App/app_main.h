/**
 * @file    app_main.h
 * @brief   System main entry. Creates all RTOS tasks.
 * @author  Ltttttts
 */

#ifndef APP_MAIN_H
#define APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 任务句柄声明 */
extern osThreadId_t ledTaskHandle;
extern osThreadId_t lcdTaskHandle;
extern osThreadId_t lvglTaskHandle;
extern osThreadId_t keyTaskHandle;
extern osThreadId_t imuTaskHandle;
extern osThreadId_t uartTaskHandle;

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H */
