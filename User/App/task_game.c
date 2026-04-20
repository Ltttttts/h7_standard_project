/**
 ******************************************************************************
 * @file    task_game.c
 * @brief   重力感应小游戏任务：重力球避障
 ******************************************************************************
 */

#include "task_game.h"
#include "lvgl.h"
#include "cmsis_os.h"
#include "bsp_jy61p.h"
#include "bsp_key.h"
#include "key_dev.h"
#include "logger.h"

static const char* TAG = "Game_Task";

// 游戏状态定义
typedef struct {
    float ball_x, ball_y;   // 小球的 X, Y 坐标
    float vel_x, vel_y;     // 小球的 X, Y 速度
    lv_obj_t * ball_obj;    // 小球的 LVGL 对象指针
    lv_obj_t * container;   // 游戏全屏容器的 LVGL 对象指针
    bool is_running;        // 游戏是否正在运行的标志位
} Game_State_t;

static Game_State_t game;
osThreadId_t game_task_handle = NULL;

extern JY61P_t MyIMU; // 引用陀螺仪数据

// 引入刚刚在 task_lvgl.c 里创建的锁
extern osMutexId_t lvgl_mutex; 

void Game_Entry(void *argument) {
    LOG_I(TAG, "Game Task Started!");
    
    // 如果锁还没建好，直接退出防死机
    if (lvgl_mutex == NULL) {
        game_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    // 1. 创建 UI 前，乖乖拿锁
    osMutexAcquire(lvgl_mutex, osWaitForever);
    game.container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(game.container, 240, 240);
    lv_obj_set_style_bg_color(game.container, lv_color_hex(0x000000), 0);
    
    game.ball_obj = lv_obj_create(game.container);
    lv_obj_set_size(game.ball_obj, 20, 20);
    lv_obj_set_style_radius(game.ball_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(game.ball_obj, lv_color_hex(0x00FF00), 0);
    osMutexRelease(lvgl_mutex); // 拿完赶紧放开

    game.ball_x = 110; game.ball_y = 110;
    game.vel_x = 0;   game.vel_y = 0;
    game.is_running = true;

    // 2. 游戏物理主循环
    while (game.is_running) {
        // 物理运算完全独立，不需要加锁，发挥双核/RTOS并发优势
        float acc_x = MyIMU.angle[1] * 0.15f; 
        float acc_y = MyIMU.angle[0] * 0.15f;
        
        game.vel_x = (game.vel_x + acc_x) * 0.96f; 
        game.vel_y = (game.vel_y + acc_y) * 0.96f;
        game.ball_x += game.vel_x;
        game.ball_y += game.vel_y;

        if(game.ball_x < 0)   { game.ball_x = 0;   game.vel_x = -game.vel_x * 0.5f; }
        if(game.ball_x > 220) { game.ball_x = 220; game.vel_x = -game.vel_x * 0.5f; }
        if(game.ball_y < 0)   { game.ball_y = 0;   game.vel_y = -game.vel_y * 0.5f; }
        if(game.ball_y > 220) { game.ball_y = 220; game.vel_y = -game.vel_y * 0.5f; }

        // 3. 把算好的坐标更新到屏幕上（必须加锁！）
        osMutexAcquire(lvgl_mutex, osWaitForever);
        lv_obj_set_pos(game.ball_obj, (int32_t)game.ball_x, (int32_t)game.ball_y);
        osMutexRelease(lvgl_mutex);

        // 游戏帧率控制：50FPS
        osDelay(20); 
    }

    // 4. 清理资源，依然需要加锁
    LOG_I(TAG, "Cleaning up game resources...");
    osMutexAcquire(lvgl_mutex, osWaitForever);
    if(game.container) lv_obj_del(game.container);
    osMutexRelease(lvgl_mutex);
    
    // 5. 安全自杀
    game_task_handle = NULL;
    vTaskDelete(NULL); 
}

/**
 * @brief 外部启动接口
 */
void Game_Start(void) {
    if (game_task_handle == NULL) {
        const osThreadAttr_t game_attr = {
            .name = "MiniGame",
            .stack_size = 1024 * 4, // 给 4KB 栈
            .priority = osPriorityNormal,
        };
        game_task_handle = osThreadNew(Game_Entry, NULL, &game_attr);
    }
}


void Game_Stop(void) {
    game.is_running = false;
}
