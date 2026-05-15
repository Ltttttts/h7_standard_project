/**
 * @file    task_lvgl.c
 * @brief   LVGL 任务，负责 UI 工厂、页面路由、显示刷新、输入路由和各功能页面。
 * @author  Ltttttts
 *
 * ============================================================
 *  本文件是 GUI 系统核心，主要包含以下模块。
 * ============================================================
 *
 *  [A] LVGL 显示刷新      (prv_disp_flush_cb)
 *     将 LVGL 渲染完成的帧缓冲写入 LCD。
 *
 *  [B] 输入设备回调        (prv_keypad_read_cb)
 *     将实体按键和串口命令转换为 LVGL 按键事件。
 *
 *  [C] UI 工厂函数          (prv_factory_*)
 *     提供统一样式的菜单创建工具。
 *
 *  [D] 页面路由             (prv_page_navigate)
 *     隐藏当前页面、显示目标页面并设置焦点。
 *
 *  [E] 输入事件路由         (prv_handle_key_message)
 *     根据当前页面分发按键行为。
 *
 *  [F] 功能页面 UI          (UI_*_Init)
 *     创建任务监视、IMU、SD、文件阅读和 LED 控制页面。
 *
 *  [G] 主菜单               (UI_MainMenu_Create)
 *     创建系统功能入口。
 *
 *  [H] 任务入口             (Task_LVGL_Entry)
 *     主循环执行消息处理、页面刷新和 LVGL tick 推进。
 *
 * ============================================================
 *  线程安全说明
 * ============================================================
 *
 *  LVGL 不是线程安全的。本文件通过 lvgl_mutex 串行化 LVGL API 调用。
 *  其他任务如果需要修改 LVGL 对象，也应先获取该互斥锁。
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
#define DISP_HOR_RES        (240U)   /* 屏幕水平分辨率。 */
#define DISP_VER_RES        (240U)   /* 屏幕垂直分辨率。 */

/* ========== 交互参数 ========== */
#define ANIM_DURATION_MS    (200U)   /* 菜单焦点动画时长。 */
#define SCROLL_STEP_FAST    (40U)    /* 单次滚动距离。 */
#define SCROLL_STEP_SLOW    (15U)    /* 长按连续滚动距离。 */
#define HIGHLIGHT_COLOR     0x0078D7 /* 菜单焦点高亮色。 */
#define HIGHLIGHT_RADIUS    (8U)     /* 高亮块圆角。 */
#define BG_DARK_COLOR       0x222222 /* 深色背景。 */
#define BG_WHITE_COLOR      0xFFFFFF /* 白色背景。 */

/* ========== IMU 参数 ========== */
#define IMU_ANGLE_RANGE     (180)    /* 欧拉角显示范围：-180 到 180。 */
#define IMU_ACC_RANGE       (2000)   /* 加速度显示范围，单位 mg。 */
#define IMU_GYRO_RANGE      (2000)   /* 角速度显示范围，单位 dps。 */
#define IMU_DATA_COUNT      (9U)     /* IMU 通道数量：角度、加速度、角速度各 3 个。 */

/* ========== LED 参数 ========== */
#define LED_INTERVAL_MAX    (5000U)  /* 状态灯闪烁最大周期。 */
#define LED_INTERVAL_STEP   (100U)   /* 状态灯闪烁周期步进。 */
#define LED_INTERVAL_MIN    (100U)   /* 状态灯闪烁最小周期。 */
#define FLOW_LED_INTERVAL_MAX  (2000U)
#define FLOW_LED_INTERVAL_STEP (50U)
#define FLOW_LED_INTERVAL_MIN  (50U)

/* ========== SD 参数 ========== */
#define FILE_NAME_MAX       (64U)     /* 文件名和路径最大长度。 */
#define FILE_READ_BUF_SIZE  (2048U)   /* 文件读取缓冲区大小。 */

/*
 * ============================================================
 *  全局变量
 * ============================================================
 */

/** LVGL 互斥锁，其他任务访问 LVGL 前需要获取。 */
osMutexId_t lvgl_mutex = NULL;

/**
 * LVGL 显示缓冲区。
 * 使用 PARTIAL 渲染模式，缓冲区大小为屏幕的 1/10。
 * 32 字节对齐便于 DMA 和缓存维护。
 */
static uint8_t draw_buf[DISP_HOR_RES * DISP_VER_RES / 10 * 2]
    __attribute__((aligned(32)));

/* 外部对象，由对应模块定义。 */
extern FATFS SDFatFs;
extern JY61P_t MyIMU;

/** 应用上下文，集中保存页面、输入、文件系统和 LED 状态。 */
static AppContext_t App;

/* 子页面初始化函数，页面首次进入时延迟创建。 */
static void UI_TaskMonitor_Init(void);
static void UI_IMUView_Init(void);
static void UI_SDExplorer_Init(void);
static void UI_FileViewer_Init(void);
static void UI_LEDSettings_Init(void);
static void UI_FlowLEDSettings_Init(void);

/* ============================================================
 *  [C] UI 工厂函数
 *
 *  提供统一样式的菜单创建工具。菜单中的第 0 个对象是焦点高亮块，
 *  后续按钮获取焦点时，高亮块会跟随按钮位置和尺寸做动画。
 * ============================================================ */

/** 菜单按钮获取焦点时使用的文字样式。 */
static lv_style_t style_btn_text;
/** 标记菜单样式是否已初始化。 */
static bool factory_style_inited = false;

/**
 * 初始化菜单统一样式。
 * 只执行一次，为按钮焦点状态设置白色文字和颜色过渡动画。
 */
static void prv_factory_init_styles(void)
{
    if (factory_style_inited) {
        return;
    }

    lv_style_init(&style_btn_text);
    /* 过渡属性列表必须以 0 结束。 */
    static const lv_style_prop_t trans_props[] = {
        LV_STYLE_TEXT_COLOR, 0
    };
    static lv_style_transition_dsc_t trans_dsc;
    lv_style_transition_dsc_init(
        &trans_dsc, trans_props,
        lv_anim_path_ease_out, ANIM_DURATION_MS, 0, NULL);
    lv_style_set_transition(&style_btn_text, &trans_dsc);
    /* 获取焦点时文字变白，默认状态保持主题样式。 */
    lv_style_set_text_color(&style_btn_text, lv_color_white());
    factory_style_inited = true;
}

/**
 * 菜单按钮获取焦点时，移动高亮块到当前按钮位置。
 */
static void prv_factory_focus_anim_cb(lv_event_t *e)
{
    /* 当前触发焦点事件的按钮。 */
    lv_obj_t *btn = lv_event_get_target(e);
    /* 按钮父对象就是菜单列表。 */
    lv_obj_t *list = lv_obj_get_parent(btn);
    /* 列表第 0 个子对象是高亮块。 */
    lv_obj_t *sel = lv_obj_get_child(list, 0);

    if (sel == NULL) {
        return;
    }

    /* 第一次获取焦点时显示高亮块。 */
    lv_obj_remove_flag(sel, LV_OBJ_FLAG_HIDDEN);

    /*
     * 同一个 lv_anim_t 结构可复用，但每次 lv_anim_start 只启动一个属性动画。
     * 这里分别对 x、y、宽度和高度启动动画。
     */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sel);
    lv_anim_set_time(&a, ANIM_DURATION_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);

    /* Y 坐标。 */
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a,
        lv_obj_get_y(sel), lv_obj_get_y(btn));
    lv_anim_start(&a);

    /* X 坐标。 */
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a,
        lv_obj_get_x(sel), lv_obj_get_x(btn));
    lv_anim_start(&a);

    /* 宽度。 */
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_width);
    lv_anim_set_values(&a,
        lv_obj_get_width(sel), lv_obj_get_width(btn));
    lv_anim_start(&a);

    /* 高度。 */
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_height);
    lv_anim_set_values(&a,
        lv_obj_get_height(sel), lv_obj_get_height(btn));
    lv_anim_start(&a);
}

/**
 * 创建带焦点高亮块的菜单列表。
 *
 * @param parent 父对象。
 * @return 菜单列表对象。
 *
 * 列表第 0 个子对象是高亮块，通过 IGNORE_LAYOUT 脱离列表布局。
 * 后续按钮从 index=1 开始。
 */
static lv_obj_t *prv_factory_create_menu(lv_obj_t *parent)
{
    prv_factory_init_styles();

    lv_obj_t *list = lv_list_create(parent);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));

    /* 底层焦点高亮块，占用列表 index=0。 */
    lv_obj_t *sel_bg = lv_obj_create(list);
    lv_obj_remove_style_all(sel_bg);           /* 清除默认样式。 */
    lv_obj_add_flag(sel_bg, LV_OBJ_FLAG_IGNORE_LAYOUT); /* 不参与列表布局。 */
    lv_obj_add_flag(sel_bg, LV_OBJ_FLAG_HIDDEN);        /* 默认隐藏。 */
    lv_obj_set_style_bg_color(sel_bg,
        lv_color_hex(HIGHLIGHT_COLOR), 0);    /* 蓝色高亮背景。 */
    lv_obj_set_style_bg_opa(sel_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sel_bg, HIGHLIGHT_RADIUS, 0);

    return list;
}

/**
 * 向菜单列表添加一个按钮。
 *
 * @param list      菜单列表。
 * @param icon      LVGL 图标字符串。
 * @param txt       按钮文本。
 * @param cb        点击回调。
 * @param user_data 回调用户数据。
 * @return 创建出的按钮对象。
 *
 * 按钮自身背景、边框和轮廓被关闭，选中效果由高亮块负责。
 */
static lv_obj_t *prv_factory_add_menu_btn(
    lv_obj_t *list,
    const char *icon,
    const char *txt,
    lv_event_cb_t cb,
    void *user_data)
{
    lv_obj_t *btn = lv_list_add_button(list, icon, txt);

    /*
     * 去掉按钮各状态下的默认背景。
     * LV_STATE_DEFAULT / FOCUSED / FOCUS_KEY / PRESSED
     */
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_bg_opa(btn, 0, LV_STATE_PRESSED);
    /* 去掉焦点轮廓。 */
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
    /* 去掉默认边框。 */
    lv_obj_set_style_border_width(btn, 0, LV_STATE_DEFAULT);

    /* 绑定焦点文字样式。 */
    lv_obj_add_style(btn, &style_btn_text, LV_STATE_FOCUSED);
    lv_obj_add_style(btn, &style_btn_text, LV_STATE_FOCUS_KEY);

    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED,
                            user_data);
    }
    /* 绑定焦点动画。 */
    lv_obj_add_event_cb(btn, prv_factory_focus_anim_cb,
                        LV_EVENT_FOCUSED, NULL);

    return btn;
}

/**
 * 清空菜单按钮，保留 index=0 的高亮块。
 *
 * SD 文件列表刷新时会使用该函数重建列表项。
 */
static void prv_factory_clean_menu(lv_obj_t *list)
{
    uint32_t cnt = lv_obj_get_child_cnt(list);

    for (uint32_t i = cnt - 1; i >= 1; i--) {
        lv_obj_t *child = lv_obj_get_child(list, i);
        /* 只删除按钮类列表项。 */
        if (lv_obj_check_type(child, &lv_button_class) ||
            lv_obj_check_type(child,
                              &lv_list_button_class)) {
            lv_obj_delete(child);
        }
    }

    /* 隐藏高亮块，等待下一次获取焦点时显示。 */
    lv_obj_t *sel_bg = lv_obj_get_child(list, 0);
    if (sel_bg != NULL) {
        lv_obj_add_flag(sel_bg, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ============================================================
 *  [D] 页面路由
 *
 *  切换页面时先隐藏所有页面，再按需延迟创建并显示目标页面。
 *  创建完成后设置键盘焦点，确保实体按键和串口按键都能继续操作。
 * ============================================================ */
static void prv_page_navigate(App_Page_e target_page)
{
    /* 先隐藏所有已创建页面。 */
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
    if (App.ui.flow_led_menu) {
        lv_obj_add_flag(App.ui.flow_led_menu, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_t *focus = NULL;

    /* 显示目标页面，并记录需要聚焦的控件。 */
    switch (target_page) {
    case PAGE_MAIN_MENU:
        lv_obj_remove_flag(App.ui.main_menu,
                           LV_OBJ_FLAG_HIDDEN);
        /* 主菜单 index=0 是高亮块，index=1 是第一个按钮。 */
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
        /* 聚焦文件列表的第一个条目。 */
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
        /* LED 菜单 index=0 是高亮块，index=1 是状态标签，index=2 是第一个按钮。 */
        focus = lv_obj_get_child(App.ui.led_menu, 2);
        break;

    case PAGE_FLOW_LED_SETTINGS:
        if (!App.ui.flow_led_menu) {
            UI_FlowLEDSettings_Init();
        }
        lv_obj_remove_flag(App.ui.flow_led_menu,
                           LV_OBJ_FLAG_HIDDEN);
        focus = lv_obj_get_child(App.ui.flow_led_menu, 2);
        break;

    case PAGE_GAME_RUNNING:
        /* 游戏页面由 task_game.c 管理 UI。 */
        break;
    }

    /* 设置键盘焦点，并触发高亮块动画。 */
    if (focus != NULL && App.input.group != NULL) {
        lv_group_focus_obj(focus);
    }
    App.router.current_page = target_page;
}

/* ============================================================
 *  [A] LVGL 显示刷新
 *
 *  LVGL 渲染出局部区域后调用该回调，px_map 为 RGB565 像素缓冲区。
 *  BSP LCD 驱动负责把该区域写入屏幕。
 * ============================================================ */
static void prv_disp_flush_cb(
    lv_display_t *disp,
    const lv_area_t *area,   /* 需要刷新的矩形区域。 */
    uint8_t *px_map)         /* 像素数据缓冲区。 */
{
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    Screen.DrawBitmap(&Screen,
                      area->x1, area->y1, w, h,
                      (uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

/* ============================================================
 *  [B] 输入设备回调
 *
 *  LVGL 每帧调用该回调读取按键状态。
 *  输入来源包括实体按键和串口模拟按键。
 * ============================================================ */
static void prv_keypad_read_cb(
    lv_indev_t *indev_drv,
    lv_indev_data_t *data)
{
    /* 串口模拟按键先上报按下，下一帧再上报释放。 */
    if (App.input.simulated_state == 1) {
        data->key = App.input.simulated_key;
        data->state = LV_INDEV_STATE_PRESSED;
        App.input.simulated_state = 2;  /* 下一帧释放。 */
        return;
    }
    if (App.input.simulated_state == 2) {
        data->key = App.input.simulated_key;
        data->state = LV_INDEV_STATE_RELEASED;
        App.input.simulated_state = 0;  /* 事件完成。 */
        return;
    }

    /* 实体按键直接读取当前 GPIO 状态。 */
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

/* ============================================================
 *  [E] 输入事件路由
 *
 *  从输入管理器收到按键事件后，根据当前页面决定是否返回、滚动页面，
 *  或转成 LVGL keypad 事件交给菜单控件处理。
 * ============================================================ */

/** 判断当前页面是否是需要手动滚动的数据查看页。 */
static bool prv_is_scroll_page(void)
{
    return (App.router.current_page == PAGE_OS_MONITOR) ||
           (App.router.current_page == PAGE_IMU_VIEW) ||
           (App.router.current_page == PAGE_FILE_READER);
}

/** 获取当前数据查看页的滚动目标对象。 */
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

/** 判断当前页面是否是可接收 LVGL keypad 事件的菜单页。 */
static bool prv_is_menu_page(void)
{
    return (App.router.current_page == PAGE_MAIN_MENU) ||
           (App.router.current_page == PAGE_LED_SETTINGS) ||
           (App.router.current_page == PAGE_FLOW_LED_SETTINGS) ||
           (App.router.current_page == PAGE_SD_EXPLORER);
}

/**
 * 返回键处理。
 *
 * 根据当前页面返回到上一级：文件阅读页返回 SD 浏览器，游戏页先停止游戏，
 * 其他子页面返回主菜单。
 */
static void prv_handle_back_key(void)
{
    if (App.router.current_page == PAGE_FILE_READER) {
        prv_page_navigate(PAGE_SD_EXPLORER);
    } else if (App.router.current_page == PAGE_GAME_RUNNING &&
               Game_IsRunning()) {
        Game_Stop();
        /* 等待游戏任务完成退出，再回到主菜单。 */
        for (int i = 0; i < 100; i++) {
            if (!Game_IsRunning()) break;
            osDelay(5);
        }
        prv_page_navigate(PAGE_MAIN_MENU);
    } else if (App.router.current_page != PAGE_MAIN_MENU) {
        prv_page_navigate(PAGE_MAIN_MENU);
    }
}

/** 处理数据查看页的单次滚动。 */
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

/** 将串口输入转换为下一帧可读取的 LVGL 模拟按键。 */
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
        App.input.simulated_state = 1;  /* 标记为刚按下。 */
    }
}

/** 统一处理输入消息。 */
static void prv_handle_key_message(InputEventMsg_t msg)
{
    /* 返回键优先处理。 */
    if (msg.key_code == SYS_KEY_BACK &&
        msg.event == KEY_EVENT_CLICK) {
        prv_handle_back_key();
        return;
    }

    /* 数据查看页使用实体上下键滚动。 */
    if (prv_is_scroll_page()) {
        prv_handle_scroll_key(msg);
    }

    /* 菜单页把串口输入转为 LVGL keypad 事件。 */
    if (prv_is_menu_page() &&
        App.input.group != NULL &&
        msg.source == INPUT_SRC_UART &&
        msg.event == KEY_EVENT_CLICK) {
        prv_handle_uart_key(msg);
    }
}

/* ============================================================
 *  [F] 各功能页面 UI
 * ============================================================ */

/* ---------- 任务监视页面 ---------- */

/**
 * 定时刷新 RTOS 任务列表。
 *
 * 使用 FreeRTOS 的 uxTaskGetSystemState() 获取任务状态，
 * 表格中显示任务名和剩余栈空间。
 */
static void prv_task_monitor_timer_cb(lv_timer_t *timer)
{
    /* 获取当前任务数量。 */
    UBaseType_t task_cnt = uxTaskGetNumberOfTasks();
    /* 为任务状态数组动态分配空间。 */
    TaskStatus_t *arr = pvPortMalloc(
        task_cnt * sizeof(TaskStatus_t));
    if (arr == NULL) {
        return;
    }

    task_cnt = uxTaskGetSystemState(arr, task_cnt, NULL);

    /* 按任务编号排序，保证显示顺序稳定。 */
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

    /* 任务数量变化时同步更新表格行数。 */
    static UBaseType_t last_cnt = 0;
    if (task_cnt != last_cnt) {
        lv_table_set_row_cnt(App.ui.task_table,
                             task_cnt + 1);
        last_cnt = task_cnt;
    }
    /* 填充表格内容。 */
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

/** 创建任务监视页面。 */
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
    /* 每秒刷新一次任务信息。 */
    lv_timer_create(prv_task_monitor_timer_cb, 1000, NULL);
}

/* ---------- IMU 数据页面 ---------- */

/**
 * IMU 数据更新定时器，100ms 刷新一次。
 *
 * 读取 MyIMU 中的角度、加速度和角速度，并更新对应标签和条形图。
 */
static void prv_imu_update_timer_cb(lv_timer_t *timer)
{
    static const char *ANGLE_FMT = "%.1f deg";
    static const char *ACC_FMT   = "%.2f g";
    static const char *GYRO_FMT  = "%.0f d/s";

    /* 页面未创建或隐藏时不刷新。 */
    if (App.ui.imu_view == NULL ||
        lv_obj_has_flag(App.ui.imu_view,
                        LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    char buf[32];
    /* 角度 3 通道。 */
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), ANGLE_FMT,
                 MyIMU.angle[i]);
        lv_label_set_text(App.ui.imu_val_labels[i], buf);
        lv_bar_set_value(App.ui.imu_bars[i],
            (int32_t)MyIMU.angle[i], LV_ANIM_ON);
    }
    /* 加速度 3 通道，单位 g，显示时转换为 mg。 */
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), ACC_FMT,
                 MyIMU.acc[i]);
        lv_label_set_text(App.ui.imu_val_labels[i + 3],
                          buf);
        lv_bar_set_value(App.ui.imu_bars[i + 3],
            (int32_t)(MyIMU.acc[i] * 1000.0f),
            LV_ANIM_ON);
    }
    /* 角速度 3 通道。 */
    for (int i = 0; i < 3; i++) {
        snprintf(buf, sizeof(buf), GYRO_FMT,
                 MyIMU.gyro[i]);
        lv_label_set_text(App.ui.imu_val_labels[i + 6],
                          buf);
        lv_bar_set_value(App.ui.imu_bars[i + 6],
            (int32_t)MyIMU.gyro[i], LV_ANIM_ON);
    }
}

/** 创建 IMU 数据页面。 */
static void UI_IMUView_Init(void)
{
    App.ui.imu_view = lv_obj_create(lv_screen_active());
    lv_obj_set_style_bg_color(App.ui.imu_view,
        lv_color_hex(BG_WHITE_COLOR), 0);
    lv_obj_set_style_pad_all(App.ui.imu_view, 0,
                             LV_PART_MAIN);
    lv_obj_set_size(App.ui.imu_view,
                    LV_PCT(100), LV_PCT(100));

    /* 使用 LVGL Grid 布局，每个通道包含名称、条形图和值标签。 */
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

    const char *titles[] = {   /* 9 个数据通道名称。 */
        "Roll:", "Pitch:", "Yaw:",
        "AccX:", "AccY:", "AccZ:",
        "GyrX:", "GyrY:", "GyrZ:"
    };

    /* 创建 9 组控件：名称、条形图和值标签。 */
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

        /* 设置对称模式和显示范围。 */
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

/* ---------- 文件阅读页面 ---------- */

/** 创建文件阅读页面。 */
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

/* ---------- SD 文件浏览页面 ---------- */

/**
 * 文件点击回调，打开并显示文件内容。
 *
 * 文件列表项文本包含大小后缀，这里先剥离后缀，再拼接完整路径。
 */
static void prv_file_click_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *lab = lv_obj_get_child(btn, 1);  /* 按钮文本标签。 */
    if (lab == NULL) {
        return;
    }

    /* 读取文件名，并去掉大小信息后缀。 */
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

    /* 打开并读取文件内容。 */
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

/** 刷新 SD 根目录文件列表。 */
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

/**
 * SD 挂载定时器回调。
 *
 * 页面创建后延迟 200ms 挂载 SD 卡。挂载成功后刷新文件列表，
 * 失败则保留状态标签显示错误信息。该定时器执行一次后删除自身。
 */
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

/** 创建 SD 文件浏览页面。 */
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

/* ---------- 状态灯设置页面 ---------- */

/** 将当前状态灯设置同步到后端任务和 UI 标签。 */
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

/** 状态灯设置按钮回调。 */
static void prv_led_btn_cb(lv_event_t *e)
{
    uint32_t action = (uint32_t)(uintptr_t)
        lv_event_get_user_data(e);

    switch (action) {
    case 1: App.led.mode = 0; break;           /* 常亮。 */
    case 2: App.led.mode = 1; break;           /* 常灭。 */
    case 3: App.led.mode = 2; break;           /* 闪烁。 */
    case 4:                                     /* 放慢闪烁。 */
        if (App.led.interval < LED_INTERVAL_MAX) {
            App.led.interval += LED_INTERVAL_STEP;
        }
        App.led.mode = 2;
        break;
    case 5:                                     /* 加快闪烁。 */
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

/** 创建状态灯设置页面。 */
static void UI_LEDSettings_Init(void)
{
    App.ui.led_menu = prv_factory_create_menu(
        lv_screen_active());

    App.ui.led_status_label = lv_label_create(
        App.ui.led_menu);
    lv_label_set_recolor(App.ui.led_status_label, true);
    prv_led_update_backend();

    /* 创建状态灯控制按钮。 */
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

/** 将当前流水灯设置同步到后端任务和 UI 标签。 */
static void prv_flow_led_update_backend(void)
{
    if (flow_led_cmd_queue != NULL) {
        FlowLedCmdMsg_t msg = {
            (FlowLedMode_e)App.flow_led.mode,
            App.flow_led.interval
        };
        osMessageQueuePut(flow_led_cmd_queue, &msg, 0, 0);
    }
    if (App.ui.flow_led_status_label != NULL) {
        const char *mode_str[] = {
            "OFF", "FORWARD", "REVERSE", "BOUNCE", "ALL BLINK"
        };
        lv_label_set_text_fmt(
            App.ui.flow_led_status_label,
            "Pattern: #0000ff %s#  [#ff0000 %lu ms#]",
            mode_str[App.flow_led.mode],
            App.flow_led.interval);
    }
}

/** 流水灯设置按钮回调。 */
static void prv_flow_led_btn_cb(lv_event_t *e)
{
    uint32_t action = (uint32_t)(uintptr_t)
        lv_event_get_user_data(e);

    switch (action) {
    case 1: App.flow_led.mode = FLOW_LED_MODE_OFF; break;
    case 2: App.flow_led.mode = FLOW_LED_MODE_FORWARD; break;
    case 3: App.flow_led.mode = FLOW_LED_MODE_REVERSE; break;
    case 4: App.flow_led.mode = FLOW_LED_MODE_BOUNCE; break;
    case 5: App.flow_led.mode = FLOW_LED_MODE_ALL_BLINK; break;
    case 6:
        if (App.flow_led.interval < FLOW_LED_INTERVAL_MAX) {
            App.flow_led.interval += FLOW_LED_INTERVAL_STEP;
        }
        break;
    case 7:
        if (App.flow_led.interval > FLOW_LED_INTERVAL_MIN) {
            App.flow_led.interval -= FLOW_LED_INTERVAL_STEP;
        }
        break;
    default:
        break;
    }
    prv_flow_led_update_backend();
}

/** 创建流水灯设置页面。 */
static void UI_FlowLEDSettings_Init(void)
{
    App.ui.flow_led_menu = prv_factory_create_menu(
        lv_screen_active());

    App.ui.flow_led_status_label = lv_label_create(
        App.ui.flow_led_menu);
    lv_label_set_recolor(App.ui.flow_led_status_label, true);
    prv_flow_led_update_backend();

    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_CLOSE, "Flow OFF",
        prv_flow_led_btn_cb, (void *)(uintptr_t)1);
    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_RIGHT, "Forward Chase",
        prv_flow_led_btn_cb, (void *)(uintptr_t)2);
    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_LEFT, "Reverse Chase",
        prv_flow_led_btn_cb, (void *)(uintptr_t)3);
    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_REFRESH, "Bounce Chase",
        prv_flow_led_btn_cb, (void *)(uintptr_t)4);
    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_EYE_OPEN, "All Blink",
        prv_flow_led_btn_cb, (void *)(uintptr_t)5);
    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_PLUS, "Flow Slower",
        prv_flow_led_btn_cb, (void *)(uintptr_t)6);
    prv_factory_add_menu_btn(App.ui.flow_led_menu,
        LV_SYMBOL_MINUS, "Flow Faster",
        prv_flow_led_btn_cb, (void *)(uintptr_t)7);
}

/* ============================================================
 *  [G] 主菜单
 *
 *  主菜单通过统一菜单工厂创建，每个按钮负责进入对应页面。
 * ============================================================ */

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

static void prv_open_flow_led_cb(lv_event_t *e)
{
    (void)e;
    prv_page_navigate(PAGE_FLOW_LED_SETTINGS);
}

/** 创建主菜单页面。 */
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
        LV_SYMBOL_REFRESH, "Flow LED Control",
        prv_open_flow_led_cb, NULL);
    prv_factory_add_menu_btn(App.ui.main_menu,
        LV_SYMBOL_WIFI, "System Settings",
        NULL, NULL);
}

/* ============================================================
 *  [H-Init] LVGL 硬件和基础对象初始化
 *
 *  该函数由 auto_init 在组件初始化阶段调用，负责初始化 LCD、LVGL、
 *  显示设备、输入设备和 LVGL 互斥锁。
 * ============================================================ */

void prv_lvgl_hardware_init(void)
{
    Screen.Init(&Screen);
    Screen.Clear(&Screen);

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

    lvgl_mutex = osMutexNew(NULL);
}

/* ============================================================
 *  [H] LVGL 任务入口
 *
 *  每轮循环依次处理输入消息、连续滚动、LVGL 定时器和系统 tick。
 * ============================================================ */
void Task_LVGL_Entry(void *argument)
{
    (void)argument;

    /* 应用层默认值。 */
    App.led.mode = 2;
    App.led.interval = 500;
    App.flow_led.mode = FLOW_LED_MODE_OFF;
    App.flow_led.interval = 200;

    /* 打开屏幕背光。 */
    Screen.SetBacklight(&Screen, 1);

    /* 创建主菜单并进入首页。 */
    UI_MainMenu_Create();
    prv_page_navigate(PAGE_MAIN_MENU);

    /* 主循环。 */
    while (1) {
        /* 保护 LVGL，避免其他任务同时修改 UI。 */
        osMutexAcquire(lvgl_mutex, osWaitForever);

        /* 读取并处理输入事件。 */
        InputEventMsg_t input_msg;
        while (input_msg_queue != NULL &&
               osMessageQueueGet(input_msg_queue,
                   &input_msg, NULL, 0) == osOK) {
            prv_handle_key_message(input_msg);
        }

        /* 数据查看页支持按键长按连续滚动。 */
        if (prv_is_scroll_page()) {
            lv_obj_t *target = prv_get_scroll_target();
            if (target != NULL) {
                static uint8_t tick = 0;
                /* 每 10 帧约 50ms 连续滚动一次。 */
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

        /* 处理 LVGL 定时器并触发刷新。 */
        lv_timer_handler();
        osMutexRelease(lvgl_mutex);

        /* 5ms 一帧，并同步推进 LVGL tick。 */
        osDelay(5);
        lv_tick_inc(5);
    }
}
