/**
 ******************************************************************************
 * @file    task_lvgl.c
 * @brief   LVGL UI 任务与逻辑控制 (包含 OS监控、IMU视图、SD文件管理器与阅读器)
 ******************************************************************************
 */

#include <stdio.h> 
#include <string.h> 
#include "task_lvgl.h"
#include "bsp_lcd.h"   
#include "logger.h"
#include "lvgl.h"
#include "cmsis_os2.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_key.h"
#include "key_dev.h" 
#include "bsp_jy61p.h"
#include "ff.h" 
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "task_game.h"
#include "task_key.h"
#include "input_manager.h"

static const char* TAG = "LVGL_Task";

osMutexId_t lvgl_mutex = NULL;

#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    240

// LVGL 渲染缓冲区 (32字节对齐，适配 H7 Cache)
static uint8_t draw_buf[MY_DISP_HOR_RES * MY_DISP_VER_RES / 10 * 2] __attribute__((aligned(32)));

#define KEY_NUM 4
Key_t Keys[KEY_NUM];

extern JY61P_t MyIMU; 
extern FATFS SDFatFs; 
extern osThreadId_t game_task_handle;

static bool is_in_monitor = false; // 子页面状态标志

/* =======================================================
 * 2. UI 容器组件声明
 * ======================================================= */
static lv_obj_t * main_menu_cont = NULL; 
static lv_obj_t * monitor_cont   = NULL;   
static lv_obj_t * imu_view_cont  = NULL;  
static lv_obj_t * sd_menu_cont   = NULL;   
static lv_obj_t * file_view_cont = NULL; 

static lv_obj_t * task_table  = NULL;     
static lv_obj_t * btn_monitor = NULL; 

static lv_group_t * g = NULL;

/* =======================================================
 * 1. 底层显示驱动
 * ======================================================= */
static void my_disp_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    Screen.DrawBitmap(&Screen, area->x1, area->y1, width, height, (uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

// 辅助函数：判断当前是否在"数据阅读模式" (需要手动像素滚动的页面)
static bool is_in_data_view_mode(void) {
    if (monitor_cont && !lv_obj_has_flag(monitor_cont, LV_OBJ_FLAG_HIDDEN)) return true;
    if (imu_view_cont && !lv_obj_has_flag(imu_view_cont, LV_OBJ_FLAG_HIDDEN)) return true;
    if (file_view_cont && !lv_obj_has_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN)) return true;
    if (game_task_handle != NULL) return true; // 游戏运行时，也要接管按键
    return false;
}

/* =======================================================
 * 3. 任务监控页面 (OS Task Monitor)
 * ======================================================= */
static void task_monitor_timer_cb(lv_timer_t * timer) {
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    TaskStatus_t *pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    if (pxTaskStatusArray != NULL) {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);
        for(UBaseType_t i = 0; i < uxArraySize - 1; i++) {
            for(UBaseType_t j = 0; j < uxArraySize - 1 - i; j++) {
                if(pxTaskStatusArray[j].xTaskNumber > pxTaskStatusArray[j+1].xTaskNumber) {
                    TaskStatus_t temp = pxTaskStatusArray[j];
                    pxTaskStatusArray[j] = pxTaskStatusArray[j+1];
                    pxTaskStatusArray[j+1] = temp;
                }
            }
        }
        static UBaseType_t last_task_count = 0;
        if (uxArraySize != last_task_count) {
            lv_table_set_row_cnt(task_table, uxArraySize + 1);
            last_task_count = uxArraySize;
        }
        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            lv_table_set_cell_value(task_table, x + 1, 0, pxTaskStatusArray[x].pcTaskName);
            lv_table_set_cell_value_fmt(task_table, x + 1, 1, "%lu B", (uint32_t)pxTaskStatusArray[x].usStackHighWaterMark * 4);
        }
        vPortFree(pxTaskStatusArray);
    }
}

void UI_TaskMonitor_Init(lv_obj_t * parent) {
    task_table = lv_table_create(parent);
    lv_obj_align(task_table, LV_ALIGN_TOP_MID, 0, 0); 
    lv_obj_set_size(task_table, 240, 240); 
    lv_table_set_col_cnt(task_table, 2);
    lv_table_set_col_width(task_table, 0, 130); 
    lv_table_set_col_width(task_table, 1, 100); 
    lv_table_set_cell_value(task_table, 0, 0, "Task Name");
    lv_table_set_cell_value(task_table, 0, 1, "Min Free");
    lv_timer_create(task_monitor_timer_cb, 1000, NULL);
}

/* =======================================================
 * 4. IMU 数据页面 (IMU Data View)
 * ======================================================= */
static lv_obj_t * imu_val_labels[9]; 
static lv_obj_t * imu_bars[9];

static void imu_update_timer_cb(lv_timer_t * timer) {
    if(imu_view_cont == NULL || lv_obj_has_flag(imu_view_cont, LV_OBJ_FLAG_HIDDEN)) return;

    char buf[32]; 
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%.1f deg", MyIMU.angle[i]); 
        lv_label_set_text(imu_val_labels[i], buf);
        lv_bar_set_value(imu_bars[i], (int32_t)MyIMU.angle[i], LV_ANIM_ON); 
    }
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%.2f g", MyIMU.acc[i]); 
        lv_label_set_text(imu_val_labels[i + 3], buf);
        lv_bar_set_value(imu_bars[i + 3], (int32_t)(MyIMU.acc[i] * 1000.0f), LV_ANIM_ON);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%.0f d/s", MyIMU.gyro[i]); 
        lv_label_set_text(imu_val_labels[i + 6], buf);
        lv_bar_set_value(imu_bars[i + 6], (int32_t)MyIMU.gyro[i], LV_ANIM_ON);
    }
}

void UI_IMUView_Init(lv_obj_t * parent) {
    static int32_t col_dsc[] = {60, 150, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, LV_GRID_TEMPLATE_LAST};
    
    lv_obj_set_grid_dsc_array(parent, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(parent, 10, 0);

    const char * titles[9] = {"Roll:", "Pitch:", "Yaw:", "AccX:", "AccY:", "AccZ:", "GyrX:", "GyrY:", "GyrZ:"};

    for(int i = 0; i < 9; i++) {
        lv_obj_t * lab_title = lv_label_create(parent);
        lv_label_set_text(lab_title, titles[i]);
        lv_obj_set_style_text_color(lab_title, lv_color_hex(0x555555), 0); 
        lv_obj_set_grid_cell(lab_title, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, i * 2, 2);

        imu_bars[i] = lv_bar_create(parent);
        lv_obj_set_size(imu_bars[i], 140, 8); 
        lv_obj_set_grid_cell(imu_bars[i], LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_END, i * 2, 1);
        lv_bar_set_mode(imu_bars[i], LV_BAR_MODE_SYMMETRICAL); 
        lv_obj_set_style_bg_color(imu_bars[i], lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_obj_set_style_bg_color(imu_bars[i], lv_color_hex(0x00AA00), LV_PART_INDICATOR);
        
        if (i < 3)      lv_bar_set_range(imu_bars[i], -180, 180);       
        else if (i < 6) lv_bar_set_range(imu_bars[i], -2000, 2000);     
        else            lv_bar_set_range(imu_bars[i], -2000, 2000);     

        imu_val_labels[i] = lv_label_create(parent);
        lv_label_set_text(imu_val_labels[i], "0.0");
        lv_obj_set_style_text_color(imu_val_labels[i], lv_color_hex(0x008800), 0); 
        lv_obj_set_grid_cell(imu_val_labels[i], LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, i * 2 + 1, 1);
    }
    lv_timer_create(imu_update_timer_cb, 100, NULL);
}

/* =======================================================
 * 5. SD 卡文件浏览器与自检页面
 * ======================================================= */
static lv_obj_t * sd_status_lab = NULL; 
static lv_obj_t * sd_file_list  = NULL;   

static BYTE sd_work_buf[512] __attribute__((aligned(32))); 
static char sd_read_buf[64]  __attribute__((aligned(32)));

static lv_obj_t * file_content_label = NULL; 
static char current_filepath[64]; 

static int SD_General_Test(void) {
    FRESULT res;
    FIL test_file;
    UINT bw, br;
    char test_data[] = "LXB723ZG-P1 SD Test OK!";

    LOG_I(TAG, "========== SD Card Hardware Test Start ==========");
    res = f_mount(&SDFatFs, "0:/", 1);
    
    if(res != FR_OK) {
        LOG_W(TAG, "Mount Failed. Attempting to format...");
        lv_label_set_text(sd_status_lab, "Formatting SD Card...\nPlease wait...");
        lv_obj_invalidate(sd_status_lab);
        lv_timer_handler(); 
        
        res = f_mkfs("0:", FM_FAT32, 0, sd_work_buf, sizeof(sd_work_buf));
        if(res == FR_OK) res = f_mount(&SDFatFs, "0:/", 1); 
        
        if(res != FR_OK) return 1;
    }

    res = f_open(&test_file, "0:/test.tmp", FA_CREATE_ALWAYS | FA_WRITE | FA_READ);
    if(res != FR_OK) return 2;
    
    f_write(&test_file, test_data, strlen(test_data), &bw);
    f_lseek(&test_file, 0); 
    f_read(&test_file, sd_read_buf, strlen(test_data), &br);
    f_close(&test_file);
    f_unlink("0:/test.tmp"); 

    if(strcmp(test_data, sd_read_buf) != 0) return 3;

    LOG_I(TAG, "========== SD Card Test PASS ==========");
    return 0; 
}

static void UI_FileViewer_Init(void) {
    if (file_view_cont != NULL) return;

    file_view_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(file_view_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(file_view_cont, lv_color_hex(0xEEEEEE), 0);
    lv_obj_add_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * title_label = lv_label_create(file_view_cont);
    lv_label_set_text(title_label, "Reading File...");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);

    file_content_label = lv_label_create(file_view_cont);
    lv_label_set_long_mode(file_content_label, LV_LABEL_LONG_WRAP); 
    lv_obj_set_width(file_content_label, 220); 
    lv_obj_align(file_content_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_color(file_content_label, lv_color_hex(0x000000), 0);
}

static void file_click_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 1); 
    if(!label) return; 

    const char * full_text = lv_label_get_text(label);
    char pure_filename[64] = {0};
    strncpy(pure_filename, full_text, sizeof(pure_filename) - 1);

    char * tail = strstr(pure_filename, "  (");
    if (tail != NULL) {
        *tail = '\0'; 
    }

    snprintf(current_filepath, sizeof(current_filepath), "0:/%s", pure_filename);
    LOG_I(TAG, "Attempting to read file: %s", current_filepath);

    UI_FileViewer_Init();

    FIL file;
    FRESULT res = f_open(&file, current_filepath, FA_READ);
    if (res == FR_OK) {
        static char read_buffer[2048] __attribute__((aligned(32))); 
        UINT br;
        
        f_read(&file, read_buffer, sizeof(read_buffer) - 1, &br);
        read_buffer[br] = '\0'; 
        
        f_close(&file);
        lv_label_set_text(file_content_label, read_buffer);
        
        lv_obj_add_flag(sd_menu_cont, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN);
        if (sd_status_lab) lv_obj_add_flag(sd_status_lab, LV_OBJ_FLAG_HIDDEN);
        
    } else {
        LOG_E(TAG, "Failed to open file! Code: %d", res);
        if (sd_status_lab) {
            char err_msg[64];
            snprintf(err_msg, sizeof(err_msg), "Open Failed! Code: %d", res);
            lv_label_set_text(sd_status_lab, err_msg);
            lv_obj_set_style_text_color(sd_status_lab, lv_color_hex(0xFF0000), 0);
            lv_obj_remove_flag(sd_status_lab, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(sd_status_lab); 
        }
    }
}

static void SD_Refresh_FileList(void) {
    FRESULT res;
    DIR dir;
    static FILINFO fno;

    lv_obj_clean(sd_file_list); 

    res = f_opendir(&dir, "0:/");
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break; 
            
            char item_text[64];
            const char * icon;

            if (fno.fattrib & AM_DIR) {
                icon = LV_SYMBOL_DIRECTORY;
                snprintf(item_text, sizeof(item_text), "%s  <DIR>", fno.fname);
            } else {
                icon = LV_SYMBOL_FILE;
                if (fno.fsize < 1024) {
                    snprintf(item_text, sizeof(item_text), "%s  (%lu B)", fno.fname, fno.fsize);
                } else {
                    snprintf(item_text, sizeof(item_text), "%s  (%lu KB)", fno.fname, fno.fsize / 1024);
                }
            }
            
            lv_obj_t * btn = lv_list_add_button(sd_file_list, icon, item_text);
            
            if (!(fno.fattrib & AM_DIR)) { 
                lv_obj_add_event_cb(btn, file_click_event_cb, LV_EVENT_CLICKED, NULL);
            }
        }
        f_closedir(&dir);
    }
}

static void sd_test_timer_cb(lv_timer_t * timer) {
    lv_label_set_text(sd_status_lab, "Step 1: Mounting/Checking...");
    lv_obj_invalidate(sd_status_lab); 

    int test_res = SD_General_Test();
    if(test_res == 0) {
        lv_label_set_text(sd_status_lab, "Test Success! Entering...");
        lv_obj_add_flag(sd_status_lab, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_remove_flag(sd_file_list, LV_OBJ_FLAG_HIDDEN); 
        SD_Refresh_FileList();
    } else {
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "SD Test Failed!\nError Code: %d", test_res);
        lv_label_set_text(sd_status_lab, err_msg);
        lv_obj_set_style_text_color(sd_status_lab, lv_color_hex(0xFF0000), 0); 
    }
    lv_timer_delete(timer); 
}

void UI_SDExplorer_Init(lv_obj_t * parent) {
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0); 

    sd_status_lab = lv_label_create(parent);
    lv_label_set_text(sd_status_lab, "Initialising SD System...");
    lv_obj_set_style_text_color(sd_status_lab, lv_color_hex(0x555555), 0);
    lv_obj_center(sd_status_lab);

    sd_file_list = lv_list_create(parent);
    lv_obj_set_size(sd_file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(sd_file_list, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(sd_file_list, 0, 0);

    lv_timer_create(sd_test_timer_cb, 100, NULL);
}

/* =======================================================
 * 6. 主页面路由与控制
 * ======================================================= */
static void close_subpage_action(void) {
    is_in_monitor = false; 
    if (monitor_cont)  lv_obj_add_flag(monitor_cont, LV_OBJ_FLAG_HIDDEN);       
    if (imu_view_cont) lv_obj_add_flag(imu_view_cont, LV_OBJ_FLAG_HIDDEN);      
    if (sd_menu_cont)  lv_obj_add_flag(sd_menu_cont, LV_OBJ_FLAG_HIDDEN);       
    if (file_view_cont) lv_obj_add_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(main_menu_cont, LV_OBJ_FLAG_HIDDEN); 
    lv_group_focus_obj(btn_monitor); 
}

static void open_monitor_event_cb(lv_event_t * e) {
    lv_obj_add_flag(main_menu_cont, LV_OBJ_FLAG_HIDDEN); 
    if(monitor_cont == NULL) {
        monitor_cont = lv_obj_create(lv_screen_active());
        lv_obj_set_style_bg_color(monitor_cont, lv_color_hex(0x222222), 0);
        lv_obj_set_style_pad_all(monitor_cont, 0, LV_PART_MAIN); 
        lv_obj_set_size(monitor_cont, LV_PCT(100), LV_PCT(100));
        UI_TaskMonitor_Init(monitor_cont);
    }
    lv_obj_remove_flag(monitor_cont, LV_OBJ_FLAG_HIDDEN); 
    is_in_monitor = true; 
}

static void open_imu_event_cb(lv_event_t * e) {
    lv_obj_add_flag(main_menu_cont, LV_OBJ_FLAG_HIDDEN); 
    if(imu_view_cont == NULL) {
        imu_view_cont = lv_obj_create(lv_screen_active());
        lv_obj_set_style_bg_color(imu_view_cont, lv_color_hex(0xFFFFFF), 0); 
        lv_obj_set_style_pad_all(imu_view_cont, 0, LV_PART_MAIN); 
        lv_obj_set_size(imu_view_cont, LV_PCT(100), LV_PCT(100));
        UI_IMUView_Init(imu_view_cont);
    }
    lv_obj_remove_flag(imu_view_cont, LV_OBJ_FLAG_HIDDEN); 
    is_in_monitor = true; 
}

static void open_sd_action(lv_event_t * e) {
    lv_obj_add_flag(main_menu_cont, LV_OBJ_FLAG_HIDDEN); 
    if(!sd_menu_cont) {
        sd_menu_cont = lv_obj_create(lv_screen_active());
        lv_obj_set_size(sd_menu_cont, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_pad_all(sd_menu_cont, 0, 0);
        UI_SDExplorer_Init(sd_menu_cont);
    }
    lv_obj_remove_flag(sd_menu_cont, LV_OBJ_FLAG_HIDDEN); 
    is_in_monitor = true; 
}

static void open_game_action(lv_event_t * e) {
    lv_obj_add_flag(main_menu_cont, LV_OBJ_FLAG_HIDDEN);
    Game_Start();
    is_in_monitor = true; 
}

void UI_MainMenu_Create(void) {
    main_menu_cont = lv_list_create(lv_screen_active());
    lv_obj_set_size(main_menu_cont, LV_PCT(100), LV_PCT(100));
    lv_obj_center(main_menu_cont);

    btn_monitor = lv_list_add_button(main_menu_cont, LV_SYMBOL_SETTINGS, "OS Task Monitor");
    lv_obj_add_event_cb(btn_monitor, open_monitor_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_imu = lv_list_add_button(main_menu_cont, LV_SYMBOL_IMAGE, "IMU Data View");
    lv_obj_add_event_cb(btn_imu, open_imu_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * btn_sd = lv_list_add_button(main_menu_cont, LV_SYMBOL_DRIVE, "SD Explorer");
    lv_obj_add_event_cb(btn_sd, open_sd_action, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_game = lv_list_add_button(main_menu_cont, LV_SYMBOL_PLAY, "Gravity MiniGame");
    lv_obj_add_event_cb(btn_game, open_game_action, LV_EVENT_CLICKED, NULL);

    lv_list_add_button(main_menu_cont, LV_SYMBOL_WIFI, "System Settings");
}

// ==========================================
// 【核心修改】虚拟输入设备状态机缓冲
// ==========================================
static uint32_t simulated_key = 0;
static uint8_t  simulated_state = 0; // 0:松开, 1:按下, 2:等待释放
lv_indev_t * indev_keypad;           // 恢复输入设备句柄

static void keypad_read_cb(lv_indev_t * indev_drv, lv_indev_data_t * data) {
    // 1. 优先处理来自【串口队列】的瞬间脉冲（将其拉长为标准的按下-松开动作）
    if (simulated_state == 1) {
        data->key = simulated_key;
        data->state = LV_INDEV_STATE_PRESSED;
        simulated_state = 2; // 保证按压状态至少维持一个 LVGL 轮询周期
        return;
    } else if (simulated_state == 2) {
        data->key = simulated_key;
        data->state = LV_INDEV_STATE_RELEASED;
        simulated_state = 0; // 动作完成
        return;
    }

    // 2. 处理【物理按键】 (依然通过优雅的抽象层，与底层引脚彻底隔离)
    if (Input_Is_Key_Held(SYS_KEY_UP)) {
        data->key = LV_KEY_PREV;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (Input_Is_Key_Held(SYS_KEY_DOWN)) {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (Input_Is_Key_Held(SYS_KEY_ENTER)) {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* =======================================================
 * 独立的按键事件路由中心
 * 说明：注意，调用此函数前，必须已经获取了 lvgl_mutex！
 * ======================================================= */
static void UI_Handle_Key_Message(InputEventMsg_t msg) 
{
    // 1. 全局检测退出/返回
    if (is_in_monitor && msg.key_code == SYS_KEY_BACK && msg.event == KEY_EVENT_CLICK) {
        if (file_view_cont && !lv_obj_has_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(sd_menu_cont, LV_OBJ_FLAG_HIDDEN);
        } 
        else if (game_task_handle != NULL) {
            Game_Stop(); 
            osDelay(50); 
            lv_obj_remove_flag(main_menu_cont, LV_OBJ_FLAG_HIDDEN);
            is_in_monitor = false;
            lv_group_focus_obj(btn_monitor); 
        } 
        else {
            close_subpage_action(); 
        }
    }
    
    // 2. 如果在数据页，自己接管上下滚动
    else if (is_in_data_view_mode()) {
        lv_obj_t * active_scroll_obj = NULL;
        if (monitor_cont && !lv_obj_has_flag(monitor_cont, LV_OBJ_FLAG_HIDDEN)) active_scroll_obj = task_table;
        else if (imu_view_cont && !lv_obj_has_flag(imu_view_cont, LV_OBJ_FLAG_HIDDEN)) active_scroll_obj = imu_view_cont;
        else if (file_view_cont && !lv_obj_has_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN)) active_scroll_obj = file_view_cont;

        if (active_scroll_obj != NULL) {
            if (msg.key_code == SYS_KEY_UP && msg.event == KEY_EVENT_CLICK) lv_obj_scroll_by(active_scroll_obj, 0, 40, LV_ANIM_ON);
            if (msg.key_code == SYS_KEY_DOWN && msg.event == KEY_EVENT_CLICK) lv_obj_scroll_by(active_scroll_obj, 0, -40, LV_ANIM_ON);
        }
    }
    
    // ==========================================================
    // 3. 【核心新增】：如果在菜单页，且由串口发送，喂给状态机
    // ==========================================================
    else if (g != NULL && msg.source == INPUT_SRC_UART && msg.event == KEY_EVENT_CLICK) {
        if (msg.key_code == SYS_KEY_UP) simulated_key = LV_KEY_PREV;
        else if (msg.key_code == SYS_KEY_DOWN) simulated_key = LV_KEY_NEXT;
        else if (msg.key_code == SYS_KEY_ENTER) simulated_key = LV_KEY_ENTER;
        else simulated_key = 0;
        
        if (simulated_key != 0) {
            simulated_state = 1; // 触发状态机，LVGL 会在下一次心跳完成动作！
        }
    }
}

void Task_LVGL_Entry(void *argument) 
{
    Screen.Init(&Screen); 
    Screen.Clear(&Screen); 
    Screen.SetBacklight(&Screen, 1);

    lv_init();
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush_cb);
    
    g = lv_group_create(); 
    lv_group_set_default(g);
    
    UI_MainMenu_Create();
    
    // 【必须恢复】：重新注册设备，LVGL拿回对焦点的控制权
    indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, keypad_read_cb);
    lv_indev_set_group(indev_keypad, g);
    
    // 创建 LVGL 全局互斥锁
    lvgl_mutex = osMutexNew(NULL);

    while(1) {
        
        // ==========================================
        osMutexAcquire(lvgl_mutex, osWaitForever);
        // ==========================================

        // 1. 处理所有排队的按键消息
        InputEventMsg_t input_msg;
        while (input_msg_queue != NULL && osMessageQueueGet(input_msg_queue, &input_msg, NULL, 0) == osOK) {
            // 【更改】：把整个 msg 结构体传进去了，包含了 source
            UI_Handle_Key_Message(input_msg);
        }
        
        // 2. 处理长按平滑滚动 (抽象轮询接口)
        if (is_in_data_view_mode()) {
            lv_obj_t * active_scroll_obj = NULL;
            if (monitor_cont && !lv_obj_has_flag(monitor_cont, LV_OBJ_FLAG_HIDDEN)) active_scroll_obj = task_table;
            else if (imu_view_cont && !lv_obj_has_flag(imu_view_cont, LV_OBJ_FLAG_HIDDEN)) active_scroll_obj = imu_view_cont;
            else if (file_view_cont && !lv_obj_has_flag(file_view_cont, LV_OBJ_FLAG_HIDDEN)) active_scroll_obj = file_view_cont;

            if (active_scroll_obj != NULL) {
                static uint8_t scroll_tick = 0;
                if (++scroll_tick >= 10) { 
                    
                    if (Input_Is_Key_Held(SYS_KEY_UP)) {
                        lv_obj_scroll_by(active_scroll_obj, 0, 15, LV_ANIM_ON);
                    }
                    if (Input_Is_Key_Held(SYS_KEY_DOWN)) {
                        lv_obj_scroll_by(active_scroll_obj, 0, -15, LV_ANIM_ON);
                    }
                    
                    scroll_tick = 0;
                }
            }
        }

        // 3. LVGL 心跳
        lv_timer_handler(); 

        // ==========================================
        osMutexRelease(lvgl_mutex);
        // ==========================================

        osDelay(5); 
        lv_tick_inc(5);
    }
}


