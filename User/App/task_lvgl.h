#ifndef __TASK_LVGL_H
#define __TASK_LVGL_H

#include "cmsis_os2.h"
#include "lvgl.h"
#include "ff.h"       // 包含 FATFS 的数据类型，如 BYTE

/* =======================================================
 * 1. 页面路由状态枚举
 * ======================================================= */
typedef enum {
    PAGE_MAIN_MENU = 0,
    PAGE_OS_MONITOR,
    PAGE_IMU_VIEW,
    PAGE_SD_EXPLORER,
    PAGE_FILE_READER,
    PAGE_GAME_RUNNING,
    PAGE_LED_SETTINGS
} App_Page_e;

/* =======================================================
 * 2. 分离的子模块结构体定义
 * ======================================================= */

// 2.1 路由控制器
typedef struct {
    App_Page_e current_page;
} AppRouter_t;

// 2.2 UI 容器与组件集合
typedef struct {
    // 页面主容器
    lv_obj_t * main_menu; 
    lv_obj_t * monitor;   
    lv_obj_t * imu_view;  
    lv_obj_t * sd_menu;   
    lv_obj_t * file_view; 
    lv_obj_t * led_menu;
    // 特定页面的关键子组件
    lv_obj_t * btn_monitor;       // 记录主菜单的第一个按钮，用于强制高亮聚焦
    lv_obj_t * task_table;        // 任务监控表格
    lv_obj_t * imu_val_labels[9]; // IMU 数值标签
    lv_obj_t * imu_bars[9];       // IMU 柱状图
    lv_obj_t * led_status_label;
} AppUI_t;

// 2.3 虚拟输入设备状态机
typedef struct {
    lv_group_t * group;
    lv_indev_t * indev;
    uint32_t     simulated_key;   // 缓存串口发来的键值
    uint8_t      simulated_state; // 0:松开, 1:按下, 2:等待释放
} AppInput_t;

// 2.4 SD卡与文件系统业务数据
typedef struct {
    BYTE       work_buf[512] __attribute__((aligned(32))); 
    char       read_buf[64]  __attribute__((aligned(32)));
    char       current_filepath[64];
    lv_obj_t * status_label; // SD卡状态提示
    lv_obj_t * file_list;    // SD卡文件列表
    lv_obj_t * content_label;// 文件阅读器内容
} AppFS_t;

typedef struct {
    uint8_t  mode;       // 0:ON, 1:OFF, 2:BLINK
    uint32_t interval;   // 当前记录的闪烁间隔
} AppLedState_t;


/* =======================================================
 * 3. 顶层大结构体 (系统上下文)
 * ======================================================= */
typedef struct {
    AppRouter_t router;
    AppUI_t       ui;
    AppInput_t    input;
    AppFS_t       fs;
    AppLedState_t led;
} AppContext_t;


// 互斥锁外部声明
extern osMutexId_t lvgl_mutex;

// 任务入口声明
void Task_LVGL_Entry(void *argument);

#endif // __TASK_LVGL_H
