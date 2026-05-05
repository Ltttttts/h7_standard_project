/**
 * @file    task_lvgl.c
 * @brief   LVGL UI task with unified routing and animation.
 * @author  Ltttttts
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

#define TAG                 "LVGL_Task"

/* ========== 显示参数 ========== */
#define DISP_HOR_RES        (240U)
#define DISP_VER_RES        (240U)

/* ========== 动画参数 ========== */
#define ANIM_DURATION_MS    (200U)
#define SCROLL_STEP_FAST    (40U)
#define SCROLL_STEP_SLOW    (15U)
#define HIGHLIGHT_COLOR     0x0078D7
#define HIGHLIGHT_RADIUS    (8U)
#define BG_DARK_COLOR       0x222222
#define BG_WHITE_COLOR      0xFFFFFF

/* ========== IMU 参数 ========== */
#define IMU_ANGLE_RANGE     (180)
#define IMU_ACC_RANGE       (2000)
#define IMU_GYRO_RANGE      (2000)
#define IMU_DATA_COUNT      (9U)

/* ========== LED 参数 ========== */
#define LED_INTERVAL_MAX    (5000U)
#define LED_INTERVAL_STEP   (100U)
#define LED_INTERVAL_MIN    (100U)

/* ========== SD 卡参数 ========== */
#define FILE_NAME_MAX       (64U)
#define FILE_READ_BUF_SIZE  (2048U)

osMutexId_t lvgl_mutex = NULL;

static uint8_t draw_buf[DISP_HOR_RES * DISP_VER_RES / 10 * 2]
    __attribute__((aligned(32)));

extern JY61P_t MyIMU;
extern FATFS SDFatFs;
extern osThreadId_t game_task_handle;

static AppContext_t App;

static void UI_TaskMonitor_Init(void);
static void UI_IMUView_Init(void);
static void UI_SDExplorer_Init(void);
static void UI_FileViewer_Init(void);
static void UI_LEDSettings_Init(void);


/* ========== UI 组件工厂 ========== */
static lv_style_t style_btn_text;
static bool factory_style_inited = false;

static void prv_factory_init_styles(void)
{
    if (factory_style_inited) {
        return;
    }

    lv_style_init(&style_btn_text);
    static const lv_style_prop_t trans_props[] = {
        LV_STYLE_TEXT_COLOR, 0
    };
    static lv_style_transition_dsc_t trans_dsc;
    lv_style_transition_dsc_init(
        &trans_dsc, trans_props,
        lv_anim_path_ease_out, ANIM_DURATION_MS, 0, NULL);
    lv_style_set_transition(&style_btn_text, &trans_dsc);
    lv_style_set_text_color(&style_btn_text, lv_color_white());
    factory_style_inited = true;
}

static void prv_factory_focus_anim_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *list = lv_obj_get_parent(btn);
    lv_obj_t *sel = lv_obj_get_child(list, 0);

    if (sel == NULL) {
        return;
    }

    lv_obj_remove_flag(sel, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sel);
    lv_anim_set_time(&a, ANIM_DURATION_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a,
        lv_obj_get_y(sel), lv_obj_get_y(btn));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a,
        lv_obj_get_x(sel), lv_obj_get_x(btn));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
    lv_anim_set_values(&a,
        lv_obj_get_width(sel), lv_obj_get_width(btn));
    lv_anim_start(&a);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_values(&a,
        lv_obj_get_height(sel), lv_obj_get_height(btn));
    lv_anim_start(&a);
}

static lv_obj_t *prv_factory_create_menu(lv_obj_t *parent)
{
    prv_factory_init_styles();

    lv_obj_t *list = lv_list_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));

    lv_obj_t *sel_bg = lv_obj_create(list);
    lv_obj_remove_style_all(sel_bg);
    lv_obj_add_flag(sel_bg, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(sel_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(sel_bg,
        lv_color_hex(HIGHLIGHT_COLOR), 0);
    lv_obj_set_style_bg_opa(sel_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sel_bg, HIGHLIGHT_RADIUS, 0);

    return list;
}

static lv_obj_t *prv_factory_add_menu_btn(
    lv_obj_t *list,
    const char *icon,
    const char *txt,
    lv_event_cb_t cb,
    void *user_data)
{
    lv_obj_t *btn = lv_list_add_button(list, icon, txt);

    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_width(btn, 0, LV_STATE_DEFAULT);

    lv_obj_add_style(btn, &style_btn_text, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_text, LV_STATE_FOCUS_KEY);

    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED,
                            user_data);
    }
    lv_obj_add_event_cb(btn, prv_factory_focus_anim_cb,
                        LV_EVENT_FOCUSED, NULL);

    return btn;
}

static void prv_factory_clean_menu(lv_obj_t *list)
{
    uint32_t cnt = lv_obj_get_child_cnt(list);

    for (uint32_t i = cnt - 1; i >= 1; i--) {
        lv_obj_t *child = lv_obj_get_child(list, i);
        if (lv_obj_check_type(child, &lv_button_class) ||
            lv_obj_check_type(child,
                              &lv_list_button_class)) {
            lv_obj_delete(child);
        }
    }

    lv_obj_t *sel_bg = lv_obj_get_child(list, 0);
    if (sel_bg != NULL) {
        lv_obj_add_flag(sel_bg, LV_OBJ_FLAG_HIDDEN);
    }
}


/* ========== 页面路由中心 ========== */
static void prv_page_navigate(App_Page_e target_page)
{
    /* 隐藏所有页面 */
    if (App.ui.main_menu) {
        lv_obj_add_flag(App.ui.main_menu, LV_OBJ_FLAG_HIDDEN);
    }
    if (App.ui.monitor) {
        lv_obj_add_flag(App.ui.monitor, LV_OBJ_FLAG_HIDDEN);
    }
    if (App.ui.imu_view) {
        lv_obj_add_flag(App.ui.imu_view, LV_OBJ_FLAG_HIDDEN);
    }
    if (App.ui.sd_menu) {
        lv_obj_add_flag(App.ui.sd_menu, LV_OBJ_FLAG_HIDDEN);
    }
    if (App.ui.file_view) {
        lv_obj_add_flag(App.ui.file_view, LV_OBJ_FLAG_HIDDEN);
    }
    if (App.ui.led_menu) {
        lv_obj_add_flag(App.ui.led_menu, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *focus = NULL;

    switch (target_page) {
    case PAGE_MAIN_MENU:
        lv_obj_remove_flag(App.ui.main_menu,
                           LV_OBJ_FLAG_HIDDEN);
        focus = lv_obj_get_child(App.ui.main_menu, 1);
        break;

    case PAGE_OS_MONITOR:
        if (!App.ui.monitor) {
            UI_TaskMonitor_Init();
        }
        lv_obj_remove_flag(App.ui.monitor,
                           LV_OBJ_FLAG_HIDDEN);
        break;

    case PAGE_IMU_VIEW:
        if (!App.ui.imu_view) {
            UI_IMUView_Init();
        }
        lv_obj_remove_flag(App.ui.imu_view,
                           LV_OBJ_FLAG_HIDDEN);
        break;

    case PAGE_SD_EXPLORER:
        if (!App.ui.sd_menu) {
            UI_SDExplorer_Init();
        }
        lv_obj_remove_flag(App.ui.sd_menu,
                           LV_OBJ_FLAG_HIDDEN);
        focus = lv_obj_get_child(App.fs.file_list, 1);
        break;

    case PAGE_FILE_READER:
        if (!App.ui.file_view) {
            UI_FileViewer_Init();
        }
        lv_obj_remove_flag(App.ui.file_view,
                           LV_OBJ_FLAG_HIDDEN);
        break;

    case PAGE_LED_SETTINGS:
        if (!App.ui.led_menu) {
            UI_LEDSettings_Init();
        }
        lv_obj_remove_flag(App.ui.led_menu,
                           LV_OBJ_FLAG_HIDDEN);
        focus = lv_obj_get_child(App.ui.led_menu, 2);
        break;

    case PAGE_GAME_RUNNING:
        break;
    }

    if (focus != NULL && App.input.group != NULL) {
        lv_group_focus_obj(focus);
    }
    App.router.current_page = target_page;
}

/* ========== 显示与输入驱动 ========== */
static void prv_disp_flush_cb(
    lv_display_t *disp,
    const lv_area_t *area,
    uint8_t *px_map)
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    Screen.DrawBitmap(&Screen,
                      area->x1, area->y1, w, h,
                      (uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

static void prv_keypad_read_cb(
    lv_indev_t *indev_drv,
    lv_indev_data_t *data)
{
    if (App.input.simulated_state == 1) {
        data->key = App.input.simulated_key;
        data->state = LV_INDEV_STATE_PRESSED;
        App.input.simulated_state = 2;
        return;
    }
    if (App.input.simulated_state == 2) {
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

/* ========== 按键事件路由 ========== */
static bool prv_is_scroll_page(void)
{
    return (App.router.current_page == PAGE_OS_MONITOR) ||
           (App.router.current_page == PAGE_IMU_VIEW) ||
           (App.router.current_page == PAGE_FILE_READER);
}

static lv_obj_t *prv_get_scroll_target(void)
{
    if (App.router.current_page == PAGE_OS_MONITOR) {
        return App.ui.task_table;
    }
    if (App.router.current_page == PAGE_IMU_VIEW) {
        return App.ui.imu_view;
    }
    if (App.router.current_page == PAGE_FILE_READER) {
        return App.ui.file_view;
    }
    return NULL;
}

static bool prv_is_menu_page(void)
{
    return (App.router.current_page == PAGE_MAIN_MENU) ||
           (App.router.current_page == PAGE_LED_SETTINGS) ||
           (App.router.current_page == PAGE_SD_EXPLORER);
}

static void prv_handle_back_key(void)
{
    if (App.router.current_page == PAGE_FILE_READER) {
        prv_page_navigate(PAGE_SD_EXPLORER);
    } else if (App.router.current_page == PAGE_GAME_RUNNING &&
               game_task_handle != NULL) {
        Game_Stop();
        osDelay(50);
        prv_page_navigate(PAGE_MAIN_MENU);
    } else if (App.router.current_page != PAGE_MAIN_MENU) {
        prv_page_navigate(PAGE_MAIN_MENU);
    }
}

static void prv_handle_scroll_key(const InputEventMsg_t msg)
{
    lv_obj_t *target = prv_get_scroll_target();
    if (target == NULL) {
        return;
    }
    if (msg.key_code == SYS_KEY_UP &&
        msg.event == KEY_EVENT_CLICK) {
        lv_obj_scroll_by(target, 0, SCROLL_STEP_FAST,
                         LV_ANIM_ON);
    }
    if (msg.key_code == SYS_KEY_DOWN &&
        msg.event == KEY_EVENT_CLICK) {
        lv_obj_scroll_by(target, 0, -SCROLL_STEP_FAST,
                         LV_ANIM_ON);
    }
}

static void prv_handle_uart_key(const InputEventMsg_t msg)
{
    if (msg.key_code == SYS_KEY_UP) {
        App.input.simulated_key = LV_KEY_PREV;
    } else if (msg.key_code == SYS_KEY_DOWN) {
        App.input.simulated_key = LV_KEY_NEXT;
    } else if (msg.key_code == SYS_KEY_ENTER) {
        App.input.simulated_key = LV_KEY_ENTER;
    } else {
        App.input.simulated_key = 0;
    }
    if (App.input.simulated_key != 0) {
        App.input.simulated_state = 1;
    }
}

static void prv_handle_key_message(InputEventMsg_t msg)
{
    if (msg.key_code == SYS_KEY_BACK &&
        msg.event == KEY_EVENT_CLICK) {
        prv_handle_back_key();
        return;
    }

    if (prv_is_scroll_page()) {
        prv_handle_scroll_key(msg);
    }

    if (prv_is_menu_page() &&
        App.input.group != NULL &&
        msg.source == INPUT_SRC_UART &&
        msg.event == KEY_EVENT_CLICK) {
        prv_handle_uart_key(msg);
    }
}

/* ========== 子页面：任务监控 ========== */
static void prv_task_monitor_timer_cb(lv_timer_t *timer)
{
    UBaseType_t task_cnt = uxTaskGetNumberOfTasks();
    TaskStatus_t *arr = pvPortMalloc(
        task_cnt * sizeof(TaskStatus_t));
    if (arr == NULL) {
        return;
    }

    task_cnt = uxTaskGetSystemState(arr, task_cnt, NULL);

    for (UBaseType_t i = 0; i < task_cnt - 1; i++) {
        for (UBaseType_t j = 0; j < task_cnt - 1 - i; j++) {
            if (arr[j].xTaskNumber >
                arr[j + 1].xTaskNumber) {
                TaskStatus_t tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
        }
    }

    static UBaseType_t last_cnt = 0;
    if (task_cnt != last_cnt) {
        lv_table_set_row_cnt(App.ui.task_table,
                             task_cnt + 1);
        last_cnt = task_cnt;
    }
    for (UBaseType_t x = 0; x < task_cnt; x++) {
        lv_table_set_cell_value(App.ui.task_table,
                                x + 1, 0,
                                arr[x].pcTaskName);
        lv_table_set_cell_value_fmt(
            App.ui.task_table, x + 1, 1, "%lu B",
            (uint32_t)arr[x].usStackHighWaterMark * 4U);
    }
    vPortFree(arr);
}

static void UI_TaskMonitor_Init(void)
{
    App.ui.monitor = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(App.ui.monitor,
        lv_color_hex(BG_DARK_COLOR), 0);
    lv_obj_set_style_pad_all(App.ui.monitor, 0,
                             LV_PART_MAIN);
    lv_obj_set_size(App.ui.monitor,
                    LV_PCT(100), LV_PCT(100));

    App.ui.task_table = lv_table_create(App.ui.monitor);
    lv_obj_align(App.ui.task_table,
                 LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(App.ui.task_table,
                    DISP_HOR_RES, DISP_VER_RES);
    lv_table_set_col_cnt(App.ui.task_table, 2);
    lv_table_set_col_width(App.ui.task_table, 0, 130);
    lv_table_set_col_width(App.ui.task_table, 1, 100);
    lv_timer_create(prv_task_monitor_timer_cb, 1000, NULL);
}

/* ========== 子页面：IMU 数据 ========== */
static void prv_imu_update_timer_cb(lv_timer_t *timer)
{
    static const char *ANGLE_FMT = "%.1f deg";
    static const char *ACC_FMT   = "%.2f g";
    static const char *GYRO_FMT  = "%.0f d/s";

    if (App.ui.imu_view == NULL ||
        lv_obj_has_flag(App.ui.imu_view,
                        LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    char buf[32];
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), ANGLE_FMT,
                 MyIMU.angle[i]);
        lv_label_set_text(App.ui.imu_val_labels[i], buf);
        lv_bar_set_value(App.ui.imu_bars[i],
            (int32_t)MyIMU.angle[i], LV_ANIM_ON);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), ACC_FMT,
                 MyIMU.acc[i]);
        lv_label_set_text(App.ui.imu_val_labels[i + 3],
                          buf);
        lv_bar_set_value(App.ui.imu_bars[i + 3],
            (int32_t)(MyIMU.acc[i] * 1000.0f),
            LV_ANIM_ON);
    }
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), GYRO_FMT,
                 MyIMU.gyro[i]);
        lv_label_set_text(App.ui.imu_val_labels[i + 6],
                          buf);
        lv_bar_set_value(App.ui.imu_bars[i + 6],
            (int32_t)MyIMU.gyro[i], LV_ANIM_ON);
    }
}

static void UI_IMUView_Init(void)
{
    App.ui.imu_view = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(App.ui.imu_view,
        lv_color_hex(BG_WHITE_COLOR), 0);
    lv_obj_set_style_pad_all(App.ui.imu_view, 0,
                             LV_PART_MAIN);
    lv_obj_set_size(App.ui.imu_view,
                    LV_PCT(100), LV_PCT(100));

    static int32_t col_dsc[] = {
        60, 150, LV_GRID_TEMPLATE_LAST
    };
    static int32_t row_dsc[] = {
        12, 20, 12, 20, 12, 20,
        12, 20, 12, 20, 12, 20,
        12, 20, 12, 20, 12, 20,
        LV_GRID_TEMPLATE_LAST
    };
    lv_obj_set_grid_dsc_array(App.ui.imu_view,
                              col_dsc, row_dsc);
    lv_obj_set_style_pad_all(App.ui.imu_view, 10, 0);

    const char *titles[] = {
        "Roll:", "Pitch:", "Yaw:",
        "AccX:", "AccY:", "AccZ:",
        "GyrX:", "GyrY:", "GyrZ:"
    };

    for (int i = 0; i < IMU_DATA_COUNT; i++) {
        lv_obj_t *lab = lv_label_create(
            App.ui.imu_view);
        lv_label_set_text(lab, titles[i]);
        lv_obj_set_grid_cell(lab,
            LV_GRID_ALIGN_START, 0, 1,
            LV_GRID_ALIGN_CENTER, i * 2, 2);

        App.ui.imu_bars[i] = lv_bar_create(
            App.ui.imu_view);
        lv_obj_set_size(App.ui.imu_bars[i], 140, 8);
        lv_obj_set_grid_cell(App.ui.imu_bars[i],
            LV_GRID_ALIGN_CENTER, 1, 1,
            LV_GRID_ALIGN_END, i * 2, 1);

        lv_bar_set_mode(App.ui.imu_bars[i],
                        LV_BAR_MODE_SYMMETRICAL);
        lv_obj_set_style_bg_color(App.ui.imu_bars[i],
            lv_color_hex(0xE0E0E0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(App.ui.imu_bars[i],
            LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(App.ui.imu_bars[i],
            lv_color_hex(0x00AA00), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(App.ui.imu_bars[i],
            LV_OPA_COVER, LV_PART_INDICATOR);

        if (i < 3) {
            lv_bar_set_range(App.ui.imu_bars[i],
                             -IMU_ANGLE_RANGE,
                             IMU_ANGLE_RANGE);
        } else {
            lv_bar_set_range(App.ui.imu_bars[i],
                             -IMU_ACC_RANGE,
                             IMU_ACC_RANGE);
        }
        lv_bar_set_value(App.ui.imu_bars[i], 0,
                         LV_ANIM_OFF);

        App.ui.imu_val_labels[i] = lv_label_create(
            App.ui.imu_view);
        lv_label_set_text(App.ui.imu_val_labels[i],
                          "0.0");
        lv_obj_set_grid_cell(App.ui.imu_val_labels[i],
            LV_GRID_ALIGN_CENTER, 1, 1,
            LV_GRID_ALIGN_START, i * 2 + 1, 1);
    }
    lv_timer_create(prv_imu_update_timer_cb, 100, NULL);
}

/* ========== 子页面：文件查看器 ========== */
static void UI_FileViewer_Init(void)
{
    App.ui.file_view = lv_obj_create(lv_screen_active());
    lv_obj_set_size(App.ui.file_view,
                    LV_PCT(100), LV_PCT(100));

    lv_obj_t *title = lv_label_create(App.ui.file_view);
    lv_label_set_text(title, "Reading File...");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    App.fs.content_label = lv_label_create(
        App.ui.file_view);
    lv_label_set_long_mode(App.fs.content_label,
                           LV_LABEL_LONG_WRAP);
    lv_obj_set_width(App.fs.content_label, 220);
    lv_obj_align(App.fs.content_label,
                 LV_ALIGN_TOP_MID, 0, 30);
}

/* ========== 子页面：SD 浏览器 ========== */
static void prv_file_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lab = lv_obj_get_child(btn, 1);
    if (lab == NULL) {
        return;
    }

    char filename[FILE_NAME_MAX] = {0};
    strncpy(filename, lv_label_get_text(lab),
            sizeof(filename) - 1);
    char *tail = strstr(filename, "  (");
    if (tail != NULL) {
        *tail = '\0';
    }

    snprintf(App.fs.current_filepath,
             sizeof(App.fs.current_filepath),
             "0:/%s", filename);
    prv_page_navigate(PAGE_FILE_READER);

    FIL file;
    if (f_open(&file, App.fs.current_filepath,
               FA_READ) == FR_OK) {
        static char buf[FILE_READ_BUF_SIZE]
            __attribute__((aligned(32)));
        UINT br;
        f_read(&file, buf, sizeof(buf) - 1, &br);
        buf[br] = '\0';
        f_close(&file);
        lv_label_set_text(App.fs.content_label, buf);
    } else {
        lv_label_set_text(App.fs.content_label,
                          "Error: Cannot open file!");
    }
}

static void prv_sd_refresh_file_list(void)
{
    DIR dir;
    static FILINFO fno;

    prv_factory_clean_menu(App.fs.file_list);

    if (f_opendir(&dir, "0:/") != FR_OK) {
        return;
    }

    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK ||
            fno.fname[0] == 0) {
            break;
        }

        char item_text[FILE_NAME_MAX];
        const char *icon;

        if (fno.fattrib & AM_DIR) {
            icon = LV_SYMBOL_DIRECTORY;
            snprintf(item_text, sizeof(item_text),
                     "%s  <DIR>", fno.fname);
        } else {
            icon = LV_SYMBOL_FILE;
            if (fno.fsize < 1024U) {
                snprintf(item_text, sizeof(item_text),
                    "%s  (%lu B)", fno.fname, fno.fsize);
            } else {
                snprintf(item_text, sizeof(item_text),
                    "%s  (%lu KB)", fno.fname,
                    fno.fsize / 1024U);
            }
        }

        lv_obj_t *btn = prv_factory_add_menu_btn(
            App.fs.file_list, icon, item_text,
            NULL, NULL);
        if (!(fno.fattrib & AM_DIR)) {
            lv_obj_add_event_cb(btn,
                prv_file_click_cb,
                LV_EVENT_CLICKED, NULL);
        }
    }
    f_closedir(&dir);
}

static void prv_sd_mount_timer_cb(lv_timer_t *timer)
{
    if (f_mount(&SDFatFs, "0:/", 1) == FR_OK) {
        lv_obj_add_flag(App.fs.status_label,
                        LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(App.fs.file_list,
                           LV_OBJ_FLAG_HIDDEN);
        prv_sd_refresh_file_list();

        lv_obj_t *first = lv_obj_get_child(
            App.fs.file_list, 1);
        if (first != NULL && App.input.group != NULL) {
            lv_group_focus_obj(first);
        }
    } else {
        lv_label_set_text(App.fs.status_label,
                          "SD Mount Failed!");
    }
    lv_timer_delete(timer);
}

static void UI_SDExplorer_Init(void)
{
    App.ui.sd_menu = lv_obj_create(lv_screen_active());
    lv_obj_set_size(App.ui.sd_menu,
                    LV_PCT(100), LV_PCT(100));

    App.fs.status_label = lv_label_create(
        App.ui.sd_menu);
    lv_label_set_text(App.fs.status_label,
                      "Mounting SD Card...");
    lv_obj_center(App.fs.status_label);

    App.fs.file_list = prv_factory_create_menu(
        App.ui.sd_menu);
    lv_obj_add_flag(App.fs.file_list,
                    LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(prv_sd_mount_timer_cb, 200, NULL);
}

/* ========== LED 设置子菜单 ========== */
static void prv_led_update_backend(void)
{
    if (led_cmd_queue != NULL) {
        LedCmdMsg_t msg = {
            (LedMode_e)App.led.mode,
            App.led.interval
        };
        osMessageQueuePut(led_cmd_queue, &msg, 0, 0);
    }
    if (App.ui.led_status_label != NULL) {
        const char *mode_str[] = {
            "ALWAYS ON", "ALWAYS OFF", "BLINKING"
        };
        lv_label_set_text_fmt(
            App.ui.led_status_label,
            "State: #0000ff %s#  [#ff0000 %lu ms#]",
            mode_str[App.led.mode],
            App.led.interval);
    }
}

static void prv_led_btn_cb(lv_event_t *e)
{
    uint32_t action = (uint32_t)(uintptr_t)
        lv_event_get_user_data(e);

    switch (action) {
    case 1: App.led.mode = 0; break;
    case 2: App.led.mode = 1; break;
    case 3: App.led.mode = 2; break;
    case 4:
        if (App.led.interval < LED_INTERVAL_MAX) {
            App.led.interval += LED_INTERVAL_STEP;
        }
        App.led.mode = 2;
        break;
    case 5:
        if (App.led.interval > LED_INTERVAL_MIN) {
            App.led.interval -= LED_INTERVAL_STEP;
        }
        App.led.mode = 2;
        break;
    default:
        break;
    }
    prv_led_update_backend();
}

static void UI_LEDSettings_Init(void)
{
    App.ui.led_menu = prv_factory_create_menu(
        lv_screen_active());

    App.ui.led_status_label = lv_label_create(
        App.ui.led_menu);
    lv_label_set_recolor(App.ui.led_status_label, true);
    prv_led_update_backend();

    prv_factory_add_menu_btn(App.ui.led_menu,
        LV_SYMBOL_POWER, "Force ON",
        prv_led_btn_cb, (void *)(uintptr_t)1);
    prv_factory_add_menu_btn(App.ui.led_menu,
        LV_SYMBOL_CLOSE, "Force OFF",
        prv_led_btn_cb, (void *)(uintptr_t)2);
    prv_factory_add_menu_btn(App.ui.led_menu,
        LV_SYMBOL_REFRESH, "Enable Blink",
        prv_led_btn_cb, (void *)(uintptr_t)3);
    prv_factory_add_menu_btn(App.ui.led_menu,
        LV_SYMBOL_PLUS, "Blink Slower",
        prv_led_btn_cb, (void *)(uintptr_t)4);
    prv_factory_add_menu_btn(App.ui.led_menu,
        LV_SYMBOL_MINUS, "Blink Faster",
        prv_led_btn_cb, (void *)(uintptr_t)5);
}

/* ========== 主菜单 ========== */
static void prv_open_monitor_cb(lv_event_t *e)
{
    (void)e;
    prv_page_navigate(PAGE_OS_MONITOR);
}

static void prv_open_imu_cb(lv_event_t *e)
{
    (void)e;
    prv_page_navigate(PAGE_IMU_VIEW);
}

static void prv_open_sd_cb(lv_event_t *e)
{
    (void)e;
    prv_page_navigate(PAGE_SD_EXPLORER);
}

static void prv_open_game_cb(lv_event_t *e)
{
    (void)e;
    Game_Start();
    prv_page_navigate(PAGE_GAME_RUNNING);
}

static void prv_open_led_cb(lv_event_t *e)
{
    (void)e;
    prv_page_navigate(PAGE_LED_SETTINGS);
}

static void UI_MainMenu_Create(void)
{
    App.ui.main_menu = prv_factory_create_menu(
        lv_screen_active());
    lv_obj_center(App.ui.main_menu);

    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_SETTINGS, "OS Task Monitor",
        prv_open_monitor_cb, NULL);
    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_IMAGE, "IMU Data View",
        prv_open_imu_cb, NULL);
    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_DRIVE, "SD Explorer",
        prv_open_sd_cb, NULL);
    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_PLAY, "Gravity MiniGame",
        prv_open_game_cb, NULL);
    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_SETTINGS, "LED Settings",
        prv_open_led_cb, NULL);
    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_WIFI, "System Settings",
        NULL, NULL);
}

/* ========== 主任务入口 ========== */
void Task_LVGL_Entry(void *argument)
{
    (void)argument;

    memset(&App, 0, sizeof(AppContext_t));
    App.led.mode = 2;
    App.led.interval = 500;

    Screen.Init(&Screen);
    Screen.Clear(&Screen);
    Screen.SetBacklight(&Screen, 1);

    lv_init();
    lv_display_t *disp = lv_display_create(
        DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_color_format(
        disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_buffers(disp, draw_buf, NULL,
        sizeof(draw_buf),
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, prv_disp_flush_cb);

    App.input.group = lv_group_create();
    lv_group_set_default(App.input.group);

    App.input.indev = lv_indev_create();
    lv_indev_set_type(App.input.indev,
                      LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(App.input.indev,
                         prv_keypad_read_cb);
    lv_indev_set_group(App.input.indev,
                       App.input.group);

    UI_MainMenu_Create();
    prv_page_navigate(PAGE_MAIN_MENU);

    lvgl_mutex = osMutexNew(NULL);

    while (1) {
        osMutexAcquire(lvgl_mutex, osWaitForever);

        InputEventMsg_t input_msg;
        while (input_msg_queue != NULL &&
               osMessageQueueGet(input_msg_queue,
                   &input_msg, NULL, 0) == osOK) {
            prv_handle_key_message(input_msg);
        }

        if (prv_is_scroll_page()) {
            lv_obj_t *target = prv_get_scroll_target();
            if (target != NULL) {
                static uint8_t tick = 0;
                if (++tick >= 10) {
                    if (Input_Is_Key_Held(SYS_KEY_UP)) {
                        lv_obj_scroll_by(target,
                            0, SCROLL_STEP_SLOW,
                            LV_ANIM_ON);
                    }
                    if (Input_Is_Key_Held(
                            SYS_KEY_DOWN)) {
                        lv_obj_scroll_by(target,
                            0, -SCROLL_STEP_SLOW,
                            LV_ANIM_ON);
                    }
                    tick = 0;
                }
            }
        }

        lv_timer_handler();
        osMutexRelease(lvgl_mutex);

        osDelay(5);
        lv_tick_inc(5);
    }
}


