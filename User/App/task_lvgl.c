/**
 ******************************************************************************
 * @file    task_lvgl.c
 * @brief   LVGL UI 任务与逻辑控制 (全局一致性 + 工厂组件化重构版)
 ******************************************************************************
 */

#include <stdio.h> 
#include <string.h> 
#include "task_lvgl.h"
#include "task_led.h"
#include "bsp_lcd.h"   
#include "logger.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_key.h"
#include "bsp_jy61p.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "task_game.h"
#include "input_manager.h"

static const char* TAG = "LVGL_Task";

osMutexId_t lvgl_mutex = NULL;

#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    240

static uint8_t draw_buf[MY_DISP_HOR_RES * MY_DISP_VER_RES / 10 * 2] __attribute__((aligned(32)));

extern JY61P_t MyIMU; 
extern FATFS SDFatFs; 
extern osThreadId_t game_task_handle;

static AppContext_t App;

// 提前声明初始化函数
static void UI_TaskMonitor_Init(void);
static void UI_IMUView_Init(void);
static void UI_SDExplorer_Init(void);
static void UI_FileViewer_Init(void);
static void UI_LEDSettings_Init(void);


/* =======================================================
 * ? UI 组件工厂 (核心扩展层：保证全局动画与样式一致性)
 * ======================================================= */
static lv_style_t style_btn_text;
static bool factory_style_inited = false;

// 工厂内部：统一样式的初始化
static void _UI_Factory_InitStyles(void) {
    if (factory_style_inited) return;
    
    lv_style_init(&style_btn_text);
    static const lv_style_prop_t trans_props[] = { LV_STYLE_TEXT_COLOR, 0 };
    static lv_style_transition_dsc_t trans_dsc;
    lv_style_transition_dsc_init(&trans_dsc, trans_props, lv_anim_path_ease_out, 200, 0, NULL);
    lv_style_set_transition(&style_btn_text, &trans_dsc);
    
    lv_style_set_text_color(&style_btn_text, lv_color_white()); // 选中时文字变白
    factory_style_inited = true;
}

// 工厂内部：全局通用的滑动追踪动画回调
static void _UI_Factory_FocusedAnimCb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * list = lv_obj_get_parent(btn);
    
    // 我们约定：工厂创建的列表，第0个子元素必定是底层高光滑块
    lv_obj_t * selector_bg = lv_obj_get_child(list, 0); 
    if(selector_bg == NULL) return;

    lv_obj_remove_flag(selector_bg, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, selector_bg);
    lv_anim_set_time(&a, 200); 
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out); 

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, lv_obj_get_y(selector_bg), lv_obj_get_y(btn));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(selector_bg), lv_obj_get_x(btn));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
    lv_anim_set_values(&a, lv_obj_get_width(selector_bg), lv_obj_get_width(btn));
    lv_anim_start(&a);
    
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_values(&a, lv_obj_get_height(selector_bg), lv_obj_get_height(btn));
    lv_anim_start(&a);
}

// 接口 1：创建一个带有底层“幽灵滑块”的动画列表
static lv_obj_t * UI_Factory_CreateMenu(lv_obj_t * parent) {
    _UI_Factory_InitStyles();
    
    lv_obj_t * list = lv_list_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));

    // 创建专属滑动高光块 (垫在最底层，Index = 0)
    lv_obj_t * selector_bg = lv_obj_create(list);
    lv_obj_remove_style_all(selector_bg);               
    lv_obj_add_flag(selector_bg, LV_OBJ_FLAG_IGNORE_LAYOUT); 
    lv_obj_add_flag(selector_bg, LV_OBJ_FLAG_HIDDEN);        
    lv_obj_set_style_bg_color(selector_bg, lv_color_hex(0x0078D7), 0); 
    lv_obj_set_style_bg_opa(selector_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(selector_bg, 8, 0);   
    
    return list;
}

// 接口 2：向动画列表中安全添加按键
static lv_obj_t * UI_Factory_AddMenuBtn(lv_obj_t * list, const char * icon, const char * txt, lv_event_cb_t cb, void * user_data) {
    lv_obj_t * btn = lv_list_add_button(list, icon, txt);
    
    // 强制点名击杀四大状态下的原生背景与边框
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_DEFAULT); 
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_FOCUSED); 
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_FOCUS_KEY); 
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_width(btn, 0, LV_STATE_DEFAULT);
    
    // 绑定颜色渐变与滑动追踪
    lv_obj_add_style(btn, &style_btn_text, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_text, LV_STATE_FOCUS_KEY);
    
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_add_event_cb(btn, _UI_Factory_FocusedAnimCb, LV_EVENT_FOCUSED, NULL); 
    
    return btn;
}

// 接口 3：安全清空列表（避开底层滑块，用于 SD 卡内容刷新）
static void UI_Factory_CleanMenu(lv_obj_t * list) {
    uint32_t child_cnt = lv_obj_get_child_cnt(list);
    // 从最后一个往前删，保留第 0 个 (底层滑块) 和第 1 个 (如果有状态标签)
    for(int i = child_cnt - 1; i >= 1; i--) {
        lv_obj_t * child = lv_obj_get_child(list, i);
        // 如果子元素不是普通按键(比如是固定的 Label)，可以选择性保留
        // 这里如果是通过 list_add_button 添加的 btn，就删掉
        if (lv_obj_check_type(child, &lv_button_class) || lv_obj_check_type(child, &lv_list_button_class)) {
             lv_obj_delete(child);
        }
    }
    // 隐藏滑块，直到下一次聚焦
    lv_obj_t * selector_bg = lv_obj_get_child(list, 0); 
    if(selector_bg) lv_obj_add_flag(selector_bg, LV_OBJ_FLAG_HIDDEN);
}


/* =======================================================
 * 1. 页面路由中心
 * ======================================================= */
static void Page_Navigate(App_Page_e target_page) 
{
    if(App.ui.main_menu) lv_obj_add_flag(App.ui.main_menu, LV_OBJ_FLAG_HIDDEN);
    if(App.ui.monitor)   lv_obj_add_flag(App.ui.monitor, LV_OBJ_FLAG_HIDDEN);
    if(App.ui.imu_view)  lv_obj_add_flag(App.ui.imu_view, LV_OBJ_FLAG_HIDDEN);
    if(App.ui.sd_menu)   lv_obj_add_flag(App.ui.sd_menu, LV_OBJ_FLAG_HIDDEN);
    if(App.ui.file_view) lv_obj_add_flag(App.ui.file_view, LV_OBJ_FLAG_HIDDEN);
    if(App.ui.led_menu)  lv_obj_add_flag(App.ui.led_menu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * target_focus_obj = NULL;

    switch(target_page) {
        case PAGE_MAIN_MENU:
            lv_obj_remove_flag(App.ui.main_menu, LV_OBJ_FLAG_HIDDEN);
            target_focus_obj = lv_obj_get_child(App.ui.main_menu, 1); // 0是背景，1是第一个按钮
            break;
            
        case PAGE_OS_MONITOR:
            if(!App.ui.monitor) UI_TaskMonitor_Init();
            lv_obj_remove_flag(App.ui.monitor, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case PAGE_IMU_VIEW:
            if(!App.ui.imu_view) UI_IMUView_Init();
            lv_obj_remove_flag(App.ui.imu_view, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case PAGE_SD_EXPLORER:
            if(!App.ui.sd_menu) UI_SDExplorer_Init();
            lv_obj_remove_flag(App.ui.sd_menu, LV_OBJ_FLAG_HIDDEN);
            // 聚焦SD列表第一个文件 (0是背景，1是文件)
            target_focus_obj = lv_obj_get_child(App.fs.file_list, 1);
            break;
            
        case PAGE_FILE_READER:
            if(!App.ui.file_view) UI_FileViewer_Init();
            lv_obj_remove_flag(App.ui.file_view, LV_OBJ_FLAG_HIDDEN);
            break;

        case PAGE_LED_SETTINGS: 
            if(!App.ui.led_menu) UI_LEDSettings_Init();
            lv_obj_remove_flag(App.ui.led_menu, LV_OBJ_FLAG_HIDDEN);
            // 聚焦LED菜单: 0是背景，1是Label提示，2是第一个按钮
            target_focus_obj = lv_obj_get_child(App.ui.led_menu, 2); 
            break;
            
        case PAGE_GAME_RUNNING:
            break; 
    }
    
    // 安全执行焦点的赋予，唤醒该页面的滑动块
    if (target_focus_obj != NULL && App.input.group != NULL) {
        lv_group_focus_obj(target_focus_obj); 
    }
    App.router.current_page = target_page;
}

/* =======================================================
 * 2. 底层显示与输入驱动 
 * ======================================================= */
static void my_disp_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;
    Screen.DrawBitmap(&Screen, area->x1, area->y1, width, height, (uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

static void keypad_read_cb(lv_indev_t * indev_drv, lv_indev_data_t * data) {
    if (App.input.simulated_state == 1) {
        data->key = App.input.simulated_key;
        data->state = LV_INDEV_STATE_PRESSED;
        App.input.simulated_state = 2; 
        return;
    } else if (App.input.simulated_state == 2) {
        data->key = App.input.simulated_key;
        data->state = LV_INDEV_STATE_RELEASED;
        App.input.simulated_state = 0; 
        return;
    }

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
 * 3. 统一按键事件路由中心
 * ======================================================= */
static void UI_Handle_Key_Message(InputEventMsg_t msg) {
    if (msg.key_code == SYS_KEY_BACK && msg.event == KEY_EVENT_CLICK) {
        if (App.router.current_page == PAGE_FILE_READER) {
            Page_Navigate(PAGE_SD_EXPLORER); 
        } 
        else if (App.router.current_page == PAGE_GAME_RUNNING && game_task_handle != NULL) {
            Game_Stop(); 
            osDelay(50); 
            Page_Navigate(PAGE_MAIN_MENU);
        } 
        else if (App.router.current_page != PAGE_MAIN_MENU) {
            Page_Navigate(PAGE_MAIN_MENU);  
        }
        return;
    }
    
    if (App.router.current_page == PAGE_OS_MONITOR || 
        App.router.current_page == PAGE_IMU_VIEW || 
        App.router.current_page == PAGE_FILE_READER) 
    {
        lv_obj_t * active_scroll_obj = NULL;
        if (App.router.current_page == PAGE_OS_MONITOR)  active_scroll_obj = App.ui.task_table;
        if (App.router.current_page == PAGE_IMU_VIEW)    active_scroll_obj = App.ui.imu_view;
        if (App.router.current_page == PAGE_FILE_READER) active_scroll_obj = App.ui.file_view;

        if (active_scroll_obj != NULL) {
            if (msg.key_code == SYS_KEY_UP && msg.event == KEY_EVENT_CLICK) lv_obj_scroll_by(active_scroll_obj, 0, 40, LV_ANIM_ON);
            if (msg.key_code == SYS_KEY_DOWN && msg.event == KEY_EVENT_CLICK) lv_obj_scroll_by(active_scroll_obj, 0, -40, LV_ANIM_ON);
        }
    }
    
    // 各种菜单页面允许串口触发按键
    if ((App.router.current_page == PAGE_MAIN_MENU || App.router.current_page == PAGE_LED_SETTINGS || App.router.current_page == PAGE_SD_EXPLORER) 
         && App.input.group != NULL && msg.source == INPUT_SRC_UART && msg.event == KEY_EVENT_CLICK) 
    {
        if (msg.key_code == SYS_KEY_UP)        App.input.simulated_key = LV_KEY_PREV;
        else if (msg.key_code == SYS_KEY_DOWN) App.input.simulated_key = LV_KEY_NEXT;
        else if (msg.key_code == SYS_KEY_ENTER)App.input.simulated_key = LV_KEY_ENTER;
        else App.input.simulated_key = 0;
        
        if (App.input.simulated_key != 0) App.input.simulated_state = 1; 
    }
}

/* =======================================================
 * 4. 各种非列表类子页面 (任务监控、IMU、文件阅读等)
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
            lv_table_set_row_cnt(App.ui.task_table, uxArraySize + 1);
            last_task_count = uxArraySize;
        }
        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            lv_table_set_cell_value(App.ui.task_table, x + 1, 0, pxTaskStatusArray[x].pcTaskName);
            lv_table_set_cell_value_fmt(App.ui.task_table, x + 1, 1, "%lu B", (uint32_t)pxTaskStatusArray[x].usStackHighWaterMark * 4);
        }
        vPortFree(pxTaskStatusArray);
    }
}

static void UI_TaskMonitor_Init(void) {
    App.ui.monitor = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(App.ui.monitor, lv_color_hex(0x222222), 0);
    lv_obj_set_style_pad_all(App.ui.monitor, 0, LV_PART_MAIN); 
    lv_obj_set_size(App.ui.monitor, LV_PCT(100), LV_PCT(100));

    App.ui.task_table = lv_table_create(App.ui.monitor);
    lv_obj_align(App.ui.task_table, LV_ALIGN_TOP_MID, 0, 0); 
    lv_obj_set_size(App.ui.task_table, 240, 240); 
    lv_table_set_col_cnt(App.ui.task_table, 2);
    lv_table_set_col_width(App.ui.task_table, 0, 130); 
    lv_table_set_col_width(App.ui.task_table, 1, 100); 
    lv_timer_create(task_monitor_timer_cb, 1000, NULL);
}

static void imu_update_timer_cb(lv_timer_t * timer) {
    if(App.ui.imu_view == NULL || lv_obj_has_flag(App.ui.imu_view, LV_OBJ_FLAG_HIDDEN)) return;
    char buf[32]; 
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%.1f deg", MyIMU.angle[i]); 
        lv_label_set_text(App.ui.imu_val_labels[i], buf);
        lv_bar_set_value(App.ui.imu_bars[i], (int32_t)MyIMU.angle[i], LV_ANIM_ON); 
    }
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%.2f g", MyIMU.acc[i]); 
        lv_label_set_text(App.ui.imu_val_labels[i + 3], buf);
        lv_bar_set_value(App.ui.imu_bars[i + 3], (int32_t)(MyIMU.acc[i] * 1000.0f), LV_ANIM_ON);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), "%.0f d/s", MyIMU.gyro[i]); 
        lv_label_set_text(App.ui.imu_val_labels[i + 6], buf);
        lv_bar_set_value(App.ui.imu_bars[i + 6], (int32_t)MyIMU.gyro[i], LV_ANIM_ON);
    }
}

static void UI_IMUView_Init(void) {
    App.ui.imu_view = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(App.ui.imu_view, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_set_style_pad_all(App.ui.imu_view, 0, LV_PART_MAIN); 
    lv_obj_set_size(App.ui.imu_view, LV_PCT(100), LV_PCT(100));

    static int32_t col_dsc[] = {60, 150, LV_GRID_TEMPLATE_LAST};
    static int32_t row_dsc[] = {12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, 12, 20, LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(App.ui.imu_view, col_dsc, row_dsc);
    lv_obj_set_style_pad_all(App.ui.imu_view, 10, 0);

    const char * titles[9] = {"Roll:", "Pitch:", "Yaw:", "AccX:", "AccY:", "AccZ:", "GyrX:", "GyrY:", "GyrZ:"};
for(int i = 0; i < 9; i++) {
        lv_obj_t * lab_title = lv_label_create(App.ui.imu_view);
        lv_label_set_text(lab_title, titles[i]);
        lv_obj_set_grid_cell(lab_title, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, i * 2, 2);

        App.ui.imu_bars[i] = lv_bar_create(App.ui.imu_view);
        lv_obj_set_size(App.ui.imu_bars[i], 140, 8); 
        lv_obj_set_grid_cell(App.ui.imu_bars[i], LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_END, i * 2, 1);
        
        // ==========================================
        // 【核心修复】：明确设置对称模式，并给出正负范围
        // ==========================================
        lv_bar_set_mode(App.ui.imu_bars[i], LV_BAR_MODE_SYMMETRICAL); 
        
        // 【新增】：有些版本的主题会覆盖背景色，我们需要强制指定一下颜色
        // 背景色（灰色）
        lv_obj_set_style_bg_color(App.ui.imu_bars[i], lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(App.ui.imu_bars[i], LV_OPA_COVER, LV_PART_MAIN);
        // 指示器颜色（绿色）
        lv_obj_set_style_bg_color(App.ui.imu_bars[i], lv_color_hex(0x00AA00), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(App.ui.imu_bars[i], LV_OPA_COVER, LV_PART_INDICATOR);

        if (i < 3)      lv_bar_set_range(App.ui.imu_bars[i], -180, 180);       // 欧拉角：-180 到 180
        else if (i < 6) lv_bar_set_range(App.ui.imu_bars[i], -2000, 2000);     // 加速度 (mg)：-2g 到 2g
        else            lv_bar_set_range(App.ui.imu_bars[i], -2000, 2000);     // 角速度 (d/s)
        
        // 【新增】：初始化时强制将值设为 0，让它先停在中间
        lv_bar_set_value(App.ui.imu_bars[i], 0, LV_ANIM_OFF);
        // ==========================================

        App.ui.imu_val_labels[i] = lv_label_create(App.ui.imu_view);
        lv_label_set_text(App.ui.imu_val_labels[i], "0.0");
        lv_obj_set_grid_cell(App.ui.imu_val_labels[i], LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, i * 2 + 1, 1);
    }
    lv_timer_create(imu_update_timer_cb, 100, NULL);
}

static void UI_FileViewer_Init(void) {
    App.ui.file_view = lv_obj_create(lv_screen_active());
    lv_obj_set_size(App.ui.file_view, LV_PCT(100), LV_PCT(100));

    lv_obj_t * title_label = lv_label_create(App.ui.file_view);
    lv_label_set_text(title_label, "Reading File...");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);

    App.fs.content_label = lv_label_create(App.ui.file_view);
    lv_label_set_long_mode(App.fs.content_label, LV_LABEL_LONG_WRAP); 
    lv_obj_set_width(App.fs.content_label, 220); 
    lv_obj_align(App.fs.content_label, LV_ALIGN_TOP_MID, 0, 30);
}


/* =======================================================
 * 5. 动态列表子页面 (SD 卡，使用了 UI 工厂)
 * ======================================================= */
static void file_click_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 1); 
    if(!label) return; 

    char pure_filename[64] = {0};
    strncpy(pure_filename, lv_label_get_text(label), sizeof(pure_filename) - 1);
    char * tail = strstr(pure_filename, "  (");
    if (tail != NULL) *tail = '\0'; 

    snprintf(App.fs.current_filepath, sizeof(App.fs.current_filepath), "0:/%s", pure_filename);
    Page_Navigate(PAGE_FILE_READER);

    FIL file;
    if (f_open(&file, App.fs.current_filepath, FA_READ) == FR_OK) {
        static char read_buffer[2048] __attribute__((aligned(32))); 
        UINT br;
        f_read(&file, read_buffer, sizeof(read_buffer) - 1, &br);
        read_buffer[br] = '\0'; 
        f_close(&file);
        lv_label_set_text(App.fs.content_label, read_buffer);
    } else {
        lv_label_set_text(App.fs.content_label, "Error: Cannot open file!");
    }
}

static void SD_Refresh_FileList(void) {
    FRESULT res;
    DIR dir;
    static FILINFO fno;

    // 使用工厂方法安全清空列表，保留底层的幽灵滑块！
    UI_Factory_CleanMenu(App.fs.file_list); 

    if (f_opendir(&dir, "0:/") == FR_OK) {
        for (;;) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break; 
            
            char item_text[64];
            const char * icon;
            if (fno.fattrib & AM_DIR) {
                icon = LV_SYMBOL_DIRECTORY;
                snprintf(item_text, sizeof(item_text), "%s  <DIR>", fno.fname);
            } else {
                icon = LV_SYMBOL_FILE;
                if (fno.fsize < 1024) snprintf(item_text, sizeof(item_text), "%s  (%lu B)", fno.fname, fno.fsize);
                else                  snprintf(item_text, sizeof(item_text), "%s  (%lu KB)", fno.fname, fno.fsize / 1024);
            }
            
            // 使用工厂方法添加按键，自动获得滑动聚焦效果！
            lv_obj_t * btn = UI_Factory_AddMenuBtn(App.fs.file_list, icon, item_text, NULL, NULL);
            if (!(fno.fattrib & AM_DIR)) { 
                lv_obj_add_event_cb(btn, file_click_event_cb, LV_EVENT_CLICKED, NULL);
            }
        }
        f_closedir(&dir);
    }
}

static void sd_test_timer_cb(lv_timer_t * timer) {
    // 简化的 SD 卡挂载逻辑 (略去硬件测试代码，加速演示)
    if(f_mount(&SDFatFs, "0:/", 1) == FR_OK) {
        lv_obj_add_flag(App.fs.status_label, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_remove_flag(App.fs.file_list, LV_OBJ_FLAG_HIDDEN); 
        SD_Refresh_FileList();
        // 挂载成功后，强制聚焦第一个文件触发高亮滑块
        lv_obj_t * first_file = lv_obj_get_child(App.fs.file_list, 1);
        if (first_file && App.input.group) lv_group_focus_obj(first_file);
    } else {
        lv_label_set_text(App.fs.status_label, "SD Mount Failed!");
    }
    lv_timer_delete(timer); 
}

static void UI_SDExplorer_Init(void) {
    App.ui.sd_menu = lv_obj_create(lv_screen_active());
    lv_obj_set_size(App.ui.sd_menu, LV_PCT(100), LV_PCT(100));

    App.fs.status_label = lv_label_create(App.ui.sd_menu);
    lv_label_set_text(App.fs.status_label, "Mounting SD Card...");
    lv_obj_center(App.fs.status_label);

    // 【核心重构】：利用工厂创建带有动画引擎的列表！
    App.fs.file_list = UI_Factory_CreateMenu(App.ui.sd_menu);
    lv_obj_add_flag(App.fs.file_list, LV_OBJ_FLAG_HIDDEN);
    
    lv_timer_create(sd_test_timer_cb, 200, NULL);
}

/* =======================================================
 * 6. LED 设置子菜单 (同样享受工厂模式的动画红利)
 * ======================================================= */
static void Update_LED_State_To_Backend(void) {
    if (led_cmd_queue != NULL) {
        LedCmdMsg_t msg = {(LedMode_e)App.led.mode, App.led.interval};
        osMessageQueuePut(led_cmd_queue, &msg, 0, 0);
    }
    if (App.ui.led_status_label != NULL) {
        const char * mode_str[] = {"ALWAYS ON", "ALWAYS OFF", "BLINKING"};
        lv_label_set_text_fmt(App.ui.led_status_label, "State: #0000ff %s#  [#ff0000 %lu ms#]", 
                              mode_str[App.led.mode], App.led.interval);
    }
}

static void led_btn_event_cb(lv_event_t * e) {
    uint32_t action_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    switch(action_id) {
        case 1: App.led.mode = 0; break; 
        case 2: App.led.mode = 1; break; 
        case 3: App.led.mode = 2; break; 
        case 4: if (App.led.interval < 5000) App.led.interval += 100; App.led.mode = 2; break; 
        case 5: if (App.led.interval > 100)  App.led.interval -= 100; App.led.mode = 2; break;
    }
    Update_LED_State_To_Backend();
}

static void UI_LEDSettings_Init(void) {
    // 【核心重构】：直接使用动画列表工厂
    App.ui.led_menu = UI_Factory_CreateMenu(lv_screen_active());

    App.ui.led_status_label = lv_label_create(App.ui.led_menu);
    lv_label_set_recolor(App.ui.led_status_label, true); 
    Update_LED_State_To_Backend(); 
    
    // 用工厂统一接口添加按键，自带样式强杀与滑动动画！
    UI_Factory_AddMenuBtn(App.ui.led_menu, LV_SYMBOL_POWER,   "Force ON", led_btn_event_cb, (void*)(uintptr_t)1);
    UI_Factory_AddMenuBtn(App.ui.led_menu, LV_SYMBOL_CLOSE,   "Force OFF", led_btn_event_cb, (void*)(uintptr_t)2);
    UI_Factory_AddMenuBtn(App.ui.led_menu, LV_SYMBOL_REFRESH, "Enable Blink", led_btn_event_cb, (void*)(uintptr_t)3);
    UI_Factory_AddMenuBtn(App.ui.led_menu, LV_SYMBOL_PLUS,    "Blink Slower", led_btn_event_cb, (void*)(uintptr_t)4);
    UI_Factory_AddMenuBtn(App.ui.led_menu, LV_SYMBOL_MINUS,   "Blink Faster", led_btn_event_cb, (void*)(uintptr_t)5);
}

/* =======================================================
 * 7. 主菜单 (所有菜单的模范代表)
 * ======================================================= */
static void open_monitor_event_cb(lv_event_t * e) { Page_Navigate(PAGE_OS_MONITOR); }
static void open_imu_event_cb(lv_event_t * e)     { Page_Navigate(PAGE_IMU_VIEW); }
static void open_sd_action(lv_event_t * e)        { Page_Navigate(PAGE_SD_EXPLORER); }
static void open_game_action(lv_event_t * e)      { Game_Start(); Page_Navigate(PAGE_GAME_RUNNING); }
static void open_led_action(lv_event_t * e)       { Page_Navigate(PAGE_LED_SETTINGS); }

static void UI_MainMenu_Create(void) {
    // 【核心重构】：主菜单也由工厂生成
    App.ui.main_menu = UI_Factory_CreateMenu(lv_screen_active());
    lv_obj_center(App.ui.main_menu);

    UI_Factory_AddMenuBtn(App.ui.main_menu, LV_SYMBOL_SETTINGS, "OS Task Monitor",  open_monitor_event_cb, NULL);
    UI_Factory_AddMenuBtn(App.ui.main_menu, LV_SYMBOL_IMAGE,    "IMU Data View",    open_imu_event_cb, NULL);
    UI_Factory_AddMenuBtn(App.ui.main_menu, LV_SYMBOL_DRIVE,    "SD Explorer",      open_sd_action, NULL);
    UI_Factory_AddMenuBtn(App.ui.main_menu, LV_SYMBOL_PLAY,     "Gravity MiniGame", open_game_action, NULL);
    UI_Factory_AddMenuBtn(App.ui.main_menu, LV_SYMBOL_SETTINGS, "LED Settings",     open_led_action, NULL);
    UI_Factory_AddMenuBtn(App.ui.main_menu, LV_SYMBOL_WIFI,     "System Settings",  NULL, NULL);
}

/* =======================================================
 * 8. 核心主任务
 * ======================================================= */
void Task_LVGL_Entry(void *argument) 
{
    memset(&App, 0, sizeof(AppContext_t));

    App.led.mode = 2;       
    App.led.interval = 500; 

    Screen.Init(&Screen); 
    Screen.Clear(&Screen); 
    Screen.SetBacklight(&Screen, 1);

    lv_init();
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush_cb);
    
    App.input.group = lv_group_create(); 
    lv_group_set_default(App.input.group);
    
    App.input.indev = lv_indev_create();
    lv_indev_set_type(App.input.indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(App.input.indev, keypad_read_cb);
    lv_indev_set_group(App.input.indev, App.input.group);
    
    UI_MainMenu_Create();
    Page_Navigate(PAGE_MAIN_MENU); 
    
    lvgl_mutex = osMutexNew(NULL);

    while(1) {
        osMutexAcquire(lvgl_mutex, osWaitForever);

        InputEventMsg_t input_msg;
        while (input_msg_queue != NULL && osMessageQueueGet(input_msg_queue, &input_msg, NULL, 0) == osOK) {
            UI_Handle_Key_Message(input_msg);
        }
        
        if (App.router.current_page == PAGE_OS_MONITOR || 
            App.router.current_page == PAGE_IMU_VIEW || 
            App.router.current_page == PAGE_FILE_READER) 
        {
            lv_obj_t * active_scroll_obj = NULL;
            if (App.router.current_page == PAGE_OS_MONITOR)  active_scroll_obj = App.ui.task_table;
            if (App.router.current_page == PAGE_IMU_VIEW)    active_scroll_obj = App.ui.imu_view;
            if (App.router.current_page == PAGE_FILE_READER) active_scroll_obj = App.ui.file_view;

            if (active_scroll_obj != NULL) {
                static uint8_t scroll_tick = 0;
                if (++scroll_tick >= 10) { 
                    if (Input_Is_Key_Held(SYS_KEY_UP))   lv_obj_scroll_by(active_scroll_obj, 0, 15, LV_ANIM_ON);
                    if (Input_Is_Key_Held(SYS_KEY_DOWN)) lv_obj_scroll_by(active_scroll_obj, 0, -15, LV_ANIM_ON);
                    scroll_tick = 0;
                }
            }
        }

        lv_timer_handler(); 
        osMutexRelease(lvgl_mutex);

        osDelay(5); 
        lv_tick_inc(5);
    }
}


