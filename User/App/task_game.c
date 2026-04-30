/**
 ******************************************************************************
 * @file    task_game.c
 * @brief   重力感应小游戏任务：引力突围 (Gravity Survivor)
 ******************************************************************************
 */

#include "task_game.h"
#include "lvgl.h"
#include "cmsis_os.h"
#include "bsp_jy61p.h"
#include "bsp_key.h"
#include "input_manager.h" // 引用 Input_Is_Key_Held
#include "logger.h"
#include <stdlib.h>        // 引入 rand()

static const char* TAG = "Game_Task";

#define SCREEN_W 240
#define SCREEN_H 240

#define MAX_ENEMIES 15

// 游戏阶段枚举
typedef enum {
    GAME_PHASE_PLAYING,
    GAME_PHASE_GAMEOVER
} GamePhase_e;

// 物理实体结构体 (以圆心坐标为基准计算)
typedef struct {
    float x, y;
    float vx, vy;
    float radius;
    lv_obj_t * obj;
    bool active;
} Entity_t;

// 全局游戏状态
typedef struct {
    GamePhase_e phase;
    bool is_running;
    int score;
    
    Entity_t player;
    Entity_t coin;
    Entity_t enemies[MAX_ENEMIES];
    
    lv_obj_t * container;
    lv_obj_t * score_label;
    lv_obj_t * over_label;
} Game_State_t;

static Game_State_t game;
osThreadId_t game_task_handle = NULL;

extern JY61P_t MyIMU; 
extern osMutexId_t lvgl_mutex; 

/* =======================================================
 * 游戏逻辑辅助函数
 * ======================================================= */

// 简单的圆与圆碰撞检测 (使用距离平方，避免开方消耗CPU)
static bool Check_Collision(Entity_t *a, Entity_t *b) {
    if (!a->active || !b->active) return false;
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dist_sq = dx * dx + dy * dy;
    float radii_sum = a->radius + b->radius;
    return dist_sq < (radii_sum * radii_sum);
}

// 随机生成实体位置 (避开玩家太近的地方)
static void Spawn_Entity_Randomly(Entity_t *ent) {
    do {
        ent->x = (float)(rand() % (SCREEN_W - 40) + 20);
        ent->y = (float)(rand() % (SCREEN_H - 40) + 20);
    } while (abs((int)(ent->x - game.player.x)) < 50 && abs((int)(ent->y - game.player.y)) < 50); // 防贴脸杀
}

// 游戏数据重置初始化
static void Game_Reset(void) {
    game.phase = GAME_PHASE_PLAYING;
    game.score = 0;
    
    // 初始化玩家
    game.player.x = SCREEN_W / 2;
    game.player.y = SCREEN_H / 2;
    game.player.vx = 0;
    game.player.vy = 0;
    
    // 初始化硬币
    Spawn_Entity_Randomly(&game.coin);
    game.coin.active = true;
    
    // 隐藏所有敌人并停用
    for(int i = 0; i < MAX_ENEMIES; i++) {
        game.enemies[i].active = false;
        // LVGL 隐藏操作必须在外部锁中进行，所以这里只改数据
    }
}

/* =======================================================
 * 核心任务入口
 * ======================================================= */
void Game_Entry(void *argument) {
    LOG_I(TAG, "Game Task Started!");
    
    if (lvgl_mutex == NULL) {
        game_task_handle = NULL;
        osThreadExit();
        return;
    }

    srand(osKernelGetTickCount()); // 初始化随机数种子

    // ==========================================
    // 1. 创建 UI 场景 (严格加锁)
    // ==========================================
    osMutexAcquire(lvgl_mutex, osWaitForever);
    
    game.container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(game.container, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(game.container, lv_color_hex(0x111111), 0);
    lv_obj_remove_style(game.container, NULL, LV_PART_SCROLLBAR); // 隐藏滚动条
    
    // 计分板
    game.score_label = lv_label_create(game.container);
    lv_obj_align(game.score_label, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_text_color(game.score_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(game.score_label, "SCORE: 0");

    // 玩家球 (绿色)
    game.player.radius = 10;
    game.player.obj = lv_obj_create(game.container);
    lv_obj_set_size(game.player.obj, game.player.radius * 2, game.player.radius * 2);
    lv_obj_set_style_radius(game.player.obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(game.player.obj, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(game.player.obj, 0, 0);
    game.player.active = true;

    // 金币 (金色)
    game.coin.radius = 6;
    game.coin.obj = lv_obj_create(game.container);
    lv_obj_set_size(game.coin.obj, game.coin.radius * 2, game.coin.radius * 2);
    lv_obj_set_style_radius(game.coin.obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(game.coin.obj, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_border_width(game.coin.obj, 0, 0);

    // 敌机池 (红色，预先创建好隐藏起来，避免运行时频繁创建内存碎片)
    for(int i = 0; i < MAX_ENEMIES; i++) {
        game.enemies[i].radius = 7;
        game.enemies[i].obj = lv_obj_create(game.container);
        lv_obj_set_size(game.enemies[i].obj, game.enemies[i].radius * 2, game.enemies[i].radius * 2);
        lv_obj_set_style_radius(game.enemies[i].obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(game.enemies[i].obj, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_border_width(game.enemies[i].obj, 0, 0);
        lv_obj_add_flag(game.enemies[i].obj, LV_OBJ_FLAG_HIDDEN);
    }

    // 死亡提示标签
    game.over_label = lv_label_create(game.container);
    lv_label_set_text(game.over_label, "GAME OVER\nPress ENTER to Restart");
    lv_obj_set_style_text_align(game.over_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(game.over_label, lv_color_hex(0xFF4444), 0);
    lv_obj_center(game.over_label);
    lv_obj_add_flag(game.over_label, LV_OBJ_FLAG_HIDDEN);

    osMutexRelease(lvgl_mutex); 

    // 初始化数据
    Game_Reset();
    game.is_running = true;

    // ==========================================
    // 2. 游戏主循环 (物理引擎与交互)
    // ==========================================
    while (game.is_running) {
        
        if (game.phase == GAME_PHASE_PLAYING) {
            
            // --- A. 玩家控制逻辑 ---
            float friction = 0.94f;
            float acc_mul = 0.15f;
            
            // 技能：按下 ENTER 键激活“冲刺”模式（打滑但加速极快）
            if (Input_Is_Key_Held(SYS_KEY_ENTER)) {
                friction = 0.98f; // 摩擦力降低，速度衰减变慢
                acc_mul = 0.35f;  // 加速度翻倍
            }

            float acc_x = MyIMU.angle[1] * acc_mul; 
            float acc_y = MyIMU.angle[0] * acc_mul;
            
            game.player.vx = (game.player.vx + acc_x) * friction; 
            game.player.vy = (game.player.vy + acc_y) * friction;
            game.player.x += game.player.vx;
            game.player.y += game.player.vy;

            // 玩家撞墙反弹
            if(game.player.x < game.player.radius) { game.player.x = game.player.radius; game.player.vx *= -0.5f; }
            if(game.player.x > SCREEN_W - game.player.radius) { game.player.x = SCREEN_W - game.player.radius; game.player.vx *= -0.5f; }
            if(game.player.y < game.player.radius) { game.player.y = game.player.radius; game.player.vy *= -0.5f; }
            if(game.player.y > SCREEN_H - game.player.radius) { game.player.y = SCREEN_H - game.player.radius; game.player.vy *= -0.5f; }

            // --- B. 敌人 AI 与移动 ---
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(!game.enemies[i].active) continue;
                
                game.enemies[i].x += game.enemies[i].vx;
                game.enemies[i].y += game.enemies[i].vy;
                
                // 敌人撞墙反弹 (完全弹性)
                if(game.enemies[i].x < game.enemies[i].radius || game.enemies[i].x > SCREEN_W - game.enemies[i].radius) 
                    game.enemies[i].vx *= -1.0f;
                if(game.enemies[i].y < game.enemies[i].radius || game.enemies[i].y > SCREEN_H - game.enemies[i].radius) 
                    game.enemies[i].vy *= -1.0f;
                    
                // 玩家与敌人碰撞检测 -> 游戏结束
                if (Check_Collision(&game.player, &game.enemies[i])) {
                    game.phase = GAME_PHASE_GAMEOVER;
                }
            }

            // --- C. 吃硬币与升级逻辑 ---
            if (Check_Collision(&game.player, &game.coin)) {
                game.score += 10;
                Spawn_Entity_Randomly(&game.coin);
                
                // 难度递增：每得 30 分，激活一个新的红球！
                int target_enemies = game.score / 30;
                if (target_enemies > MAX_ENEMIES) target_enemies = MAX_ENEMIES;
                
                for(int i = 0; i < target_enemies; i++) {
                    if(!game.enemies[i].active) {
                        game.enemies[i].active = true;
                        Spawn_Entity_Randomly(&game.enemies[i]);
                        // 给敌机一个随机的初始速度
                        game.enemies[i].vx = ((rand() % 50) / 10.0f) - 2.5f; 
                        game.enemies[i].vy = ((rand() % 50) / 10.0f) - 2.5f;
                    }
                }
            }
            
        } 
        else if (game.phase == GAME_PHASE_GAMEOVER) {
            // 死亡状态，等待按键重启
            if (Input_Is_Key_Held(SYS_KEY_ENTER)) {
                Game_Reset();
            }
        }

        // ==========================================
        // 3. 将物理坐标同步到显示层 (严格加锁)
        // ==========================================
        osMutexAcquire(lvgl_mutex, osWaitForever);
        
        if (game.phase == GAME_PHASE_PLAYING) {
            lv_obj_add_flag(game.over_label, LV_OBJ_FLAG_HIDDEN); // 隐藏死亡提示
            
            // 更新计分板
            lv_label_set_text_fmt(game.score_label, "SCORE: %d", game.score);
            
            // LVGL 绘图坐标是左上角，物理引擎是中心点，需要转换
            lv_obj_set_pos(game.player.obj, (int32_t)(game.player.x - game.player.radius), (int32_t)(game.player.y - game.player.radius));
            lv_obj_set_pos(game.coin.obj, (int32_t)(game.coin.x - game.coin.radius), (int32_t)(game.coin.y - game.coin.radius));
            
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if (game.enemies[i].active) {
                    lv_obj_remove_flag(game.enemies[i].obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_pos(game.enemies[i].obj, (int32_t)(game.enemies[i].x - game.enemies[i].radius), (int32_t)(game.enemies[i].y - game.enemies[i].radius));
                } else {
                    lv_obj_add_flag(game.enemies[i].obj, LV_OBJ_FLAG_HIDDEN);
                }
            }
        } 
        else {
            // 显示 Game Over
            lv_obj_remove_flag(game.over_label, LV_OBJ_FLAG_HIDDEN);
        }

        osMutexRelease(lvgl_mutex);

        // 保持游戏帧率稳定 (20ms = 50FPS)
        osDelay(20); 
    }

    // ==========================================
    // 4. 清理资源，准备退出
    // ==========================================
    LOG_I(TAG, "Cleaning up game resources...");
    osMutexAcquire(lvgl_mutex, osWaitForever);
    if(game.container) {
        lv_obj_del(game.container); // LVGL 删除父容器会自动递归删除所有子对象，极度省心！
        game.container = NULL;
    }
    osMutexRelease(lvgl_mutex);
    
    game_task_handle = NULL;
    osThreadExit();
}

/**
 * @brief 外部启动接口 (由主菜单点击触发)
 */
void Game_Start(void) {
    if (game_task_handle == NULL) {
        const osThreadAttr_t game_attr = {
            .name = "MiniGame",
            .stack_size = 1024 * 4,
            .priority = osPriorityNormal,
        };
        game_task_handle = osThreadNew(Game_Entry, NULL, &game_attr);
    }
}

/**
 * @brief 外部强制停止接口 (由 UI 返回键触发)
 */
void Game_Stop(void) {
    game.is_running = false;
}

