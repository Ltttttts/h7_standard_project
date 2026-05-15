/**
 * @file    task_lvgl.h
 * @brief   LVGL 任务头文件，定义页面枚举、UI 状态和全局接口。
 * @author  Ltttttts
 */

#ifndef __TASK_LVGL_H
#define __TASK_LVGL_H

#include "cmsis_os2.h"
#include "lvgl.h"
#include "ff.h"       /* FATFS 类型定义，例如 BYTE。 */

/* 页面路由状态。 */
typedef enum {
    PAGE_MAIN_MENU = 0,   /* 主菜单。 */
    PAGE_OS_MONITOR,      /* RTOS 任务监视页面。 */
    PAGE_IMU_VIEW,        /* IMU 数据显示页面。 */
    PAGE_SD_EXPLORER,     /* SD 卡文件浏览页面。 */
    PAGE_FILE_READER,     /* 文本文件阅读页面。 */
    PAGE_GAME_RUNNING,    /* 游戏运行页面，由 task_game.c 管理。 */
    PAGE_LED_SETTINGS,    /* 状态灯控制页面。 */
    PAGE_FLOW_LED_SETTINGS /* 流水灯控制页面。 */
} App_Page_e;

/* 路由状态，仅记录当前页面。 */
typedef struct {
    App_Page_e current_page;
} AppRouter_t;

/* LVGL 页面和控件对象集合。 */
typedef struct {
    lv_obj_t * main_menu;
    lv_obj_t * monitor;
    lv_obj_t * imu_view;
    lv_obj_t * sd_menu;
    lv_obj_t * file_view;
    lv_obj_t * led_menu;
    lv_obj_t * flow_led_menu;

    lv_obj_t * task_table;
    lv_obj_t * imu_val_labels[9];
    lv_obj_t * imu_bars[9];
    lv_obj_t * led_status_label;
    lv_obj_t * flow_led_status_label;
} AppUI_t;

/* 输入设备状态，用于实体按键和串口模拟按键。 */
typedef struct {
    lv_group_t * group;
    lv_indev_t * indev;
    uint32_t     simulated_key;
    uint8_t      simulated_state; /* 0: 空闲，1: 按下，2: 等待释放。 */
} AppInput_t;

/* SD 文件系统页面状态。 */
typedef struct {
    BYTE       work_buf[512] __attribute__((aligned(32)));
    char       read_buf[64]  __attribute__((aligned(32)));
    char       current_filepath[64];
    lv_obj_t * status_label;
    lv_obj_t * file_list;
    lv_obj_t * content_label;
} AppFS_t;

/* 状态灯 UI 状态。 */
typedef struct {
    uint8_t  mode;       /* 0: 常亮，1: 常灭，2: 闪烁。 */
    uint32_t interval;   /* 闪烁周期，单位 ms。 */
} AppLedState_t;

/* 流水灯 UI 状态。 */
typedef struct {
    uint8_t  mode;
    uint32_t interval;
} AppFlowLedState_t;

/* 应用上下文，集中保存 LVGL 相关状态。 */
typedef struct {
    AppRouter_t       router;
    AppUI_t           ui;
    AppInput_t        input;
    AppFS_t           fs;
    AppLedState_t     led;
    AppFlowLedState_t flow_led;
} AppContext_t;

/* LVGL 互斥锁，所有跨任务 LVGL 调用都应先获取该锁。 */
extern osMutexId_t lvgl_mutex;

/* LVGL 任务入口，由 app_main.c 在 APP 初始化阶段创建。 */
void Task_LVGL_Entry(void *argument);

#endif /* __TASK_LVGL_H */
