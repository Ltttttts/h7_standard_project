/**
 * @file    task_game.c
 * @brief   Gravity Survivor with power-up system.
 * @author  Ltttttts
 */

#include "task_game.h"
#include "lvgl.h"
#include "cmsis_os.h"
#include "bsp_jy61p.h"
#include "bsp_key.h"
#include "input_manager.h"
#include "logger.h"
#include <stdlib.h>

static const char* TAG = "Game_Task";

#define SCREEN_W 240
#define SCREEN_H 240
#define MAX_ENEMIES 15

/* 道具持续时间（帧，50FPS） */
#define SHIELD_DURATION     (150U)
#define SLOW_DURATION       (250U)
#define MAGNET_DURATION     (250U)

/* 道具掉落概率（百分比） */
#define POWERUP_DROP_CHANCE (15U)

/* 拖尾长度 */
#define TRAIL_LENGTH        (8U)

typedef enum {
    GAME_PHASE_PLAYING,
    GAME_PHASE_GAMEOVER
} GamePhase_e;

typedef enum {
    POWERUP_NONE = 0,
    POWERUP_SHIELD,
    POWERUP_SLOW,
    POWERUP_MAGNET,
    POWERUP_BOMB
} PowerUpType_e;

typedef struct {
    float x, y;
    float vx, vy;
    float radius;
    lv_obj_t * obj;
    bool active;
} Entity_t;

typedef struct {
    PowerUpType_e type;     /* 道具类型 */
    bool active;            /* 是否出现在场上 */
    float x, y;             /* 位置 */
    float radius;           /* 半径 */
    lv_obj_t * obj;         /* LVGL 对象 */
} PowerUpDrop_t;

typedef struct {
    int shield_frames;      /* 护盾剩余帧，0=无 */
    int slow_frames;        /* 时缓剩余帧，0=无 */
    int magnet_frames;      /* 吸铁石剩余帧，0=无 */
    lv_obj_t * status_label;/* 道具状态文字 */
} ActivePowerUp_t;

typedef struct {
    GamePhase_e phase;
    bool is_running;
    int score;
    int combo_count;
    int popup_value;
    int popup_life;
    lv_obj_t *popup_label;
    int pulse_frame;
    Entity_t player;
    Entity_t coin;
    Entity_t enemies[MAX_ENEMIES];
    PowerUpDrop_t powerup;  /* 掉落场上的道具 */
    ActivePowerUp_t active; /* 已激活的道具状态 */
    float trail_x[TRAIL_LENGTH];    /* 拖尾位置缓存 */
    float trail_y[TRAIL_LENGTH];
    uint8_t trail_head;
    lv_obj_t *trail_dots[TRAIL_LENGTH];
    lv_obj_t * container;
    lv_obj_t * score_label;
    lv_obj_t * combo_label;
    lv_obj_t * over_label;
} Game_State_t;

static Game_State_t game;
static osThreadId_t game_task_handle = NULL;

extern JY61P_t MyIMU;
extern osMutexId_t lvgl_mutex;

#define GAME_MUTEX_TIMEOUT_MS   (50U)

static bool prv_lock_lvgl(void)
{
    return (osMutexAcquire(lvgl_mutex,
            GAME_MUTEX_TIMEOUT_MS) == osOK);
}

static void prv_unlock_lvgl(void)
{
    osMutexRelease(lvgl_mutex);
}

static bool Check_Collision(Entity_t *a, Entity_t *b)
{
    if (!a->active || !b->active) return false;
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dist_sq = dx * dx + dy * dy;
    float radii_sum = a->radius + b->radius;
    return dist_sq < (radii_sum * radii_sum);
}

static void Spawn_Entity_Randomly(Entity_t *ent)
{
    for (int attempt = 0; attempt < 50; attempt++) {
        ent->x = (float)(rand() % (SCREEN_W - 40) + 20);
        ent->y = (float)(rand() % (SCREEN_H - 40) + 20);
        if (abs((int)(ent->x - game.player.x)) >= 50 &&
            abs((int)(ent->y - game.player.y)) >= 50) {
            return;
        }
    }
}

static uint32_t pu_color(PowerUpType_e t)
{
    switch (t) {
    case POWERUP_SHIELD: return 0x4488FF;
    case POWERUP_SLOW:   return 0xAA44FF;
    case POWERUP_MAGNET: return 0xFF8800;
    case POWERUP_BOMB:   return 0xFF4444;
    default:             return 0xFFFFFF;
    }
}

static void Spawn_PowerUp(void)
{
    PowerUpDrop_t *p = &game.powerup;
    p->type = (PowerUpType_e)(rand() % 4 + 1);
    p->active = true;
    p->radius = 8;
    for (int a = 0; a < 50; a++) {
        p->x = (float)(rand() % (SCREEN_W - 40) + 20);
        p->y = (float)(rand() % (SCREEN_H - 40) + 20);
        if (abs((int)(p->x - game.player.x)) >= 50 &&
            abs((int)(p->y - game.player.y)) >= 50) {
            return;
        }
    }
}

static void Activate_PowerUp(PowerUpType_e t)
{
    switch (t) {
    case POWERUP_SHIELD:
        game.active.shield_frames = SHIELD_DURATION;
        break;
    case POWERUP_SLOW:
        game.active.slow_frames = SLOW_DURATION;
        break;
    case POWERUP_MAGNET:
        game.active.magnet_frames = MAGNET_DURATION;
        break;
    case POWERUP_BOMB:
        for (int i = 0; i < MAX_ENEMIES; i++) {
            game.enemies[i].active = false;
        }
        break;
    default:
        break;
    }
}

static void Game_Reset(void)
{
    game.phase = GAME_PHASE_PLAYING;
    game.score = 0;
    game.combo_count = 0;
    game.popup_life = 0;
    game.pulse_frame = 0;
    game.powerup.active = false;
    game.active.shield_frames = 0;
    game.active.slow_frames = 0;
    game.active.magnet_frames = 0;
    game.trail_head = 0;
    game.player.x = SCREEN_W / 2;
    game.player.y = SCREEN_H / 2;
    game.player.vx = 0;
    game.player.vy = 0;
    game.player.active = true;
    Spawn_Entity_Randomly(&game.coin);
    game.coin.active = true;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        game.enemies[i].active = false;
    }
}

void Game_Entry(void *argument)
{
    (void)argument;

    if (lvgl_mutex == NULL) {
        game_task_handle = NULL;
        osThreadExit();
        return;
    }

    srand(osKernelGetTickCount());

    if (!prv_lock_lvgl()) {
        game_task_handle = NULL;
        osThreadExit();
        return;
    }

    game.container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(game.container, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(game.container,
        lv_color_hex(0x111111), 0);
    lv_obj_remove_style(game.container, NULL,
                        LV_PART_SCROLLBAR);

    game.score_label = lv_label_create(game.container);
    lv_obj_align(game.score_label,
                 LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_text_color(game.score_label,
        lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(game.score_label, "SCORE: 0");

    game.combo_label = lv_label_create(game.container);
    lv_obj_align(game.combo_label,
                 LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_text_color(game.combo_label,
        lv_color_hex(0xFFD700), 0);
    lv_label_set_text(game.combo_label, "");
    lv_obj_add_flag(game.combo_label,
                    LV_OBJ_FLAG_HIDDEN);

    game.popup_label = lv_label_create(game.container);
    lv_obj_set_style_text_color(game.popup_label,
        lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(game.popup_label,
                              LV_OPA_COVER, 0);
    lv_label_set_text(game.popup_label, "");
    lv_obj_add_flag(game.popup_label,
                    LV_OBJ_FLAG_HIDDEN);

    /* 道具状态条（右下） */
    game.active.status_label =
        lv_label_create(game.container);
    lv_obj_align(game.active.status_label,
                 LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    lv_obj_set_style_text_color(
        game.active.status_label,
        lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(game.active.status_label, "");
    lv_obj_add_flag(game.active.status_label,
                    LV_OBJ_FLAG_HIDDEN);

    /* 玩家 */
    game.player.radius = 10;
    game.player.obj = lv_obj_create(game.container);
    lv_obj_set_size(game.player.obj,
        game.player.radius * 2, game.player.radius * 2);
    lv_obj_set_style_radius(game.player.obj,
                            LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(game.player.obj,
        lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(game.player.obj, 0, 0);
    game.player.active = true;

    /* 拖尾粒子 */
    for (int i = 0; i < TRAIL_LENGTH; i++) {
        game.trail_dots[i] = lv_obj_create(
            game.container);
        lv_obj_set_size(game.trail_dots[i],
            game.player.radius * 2,
            game.player.radius * 2);
        lv_obj_set_style_radius(
            game.trail_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(
            game.trail_dots[i],
            lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_border_width(
            game.trail_dots[i], 0, 0);
        lv_obj_add_flag(game.trail_dots[i],
                        LV_OBJ_FLAG_HIDDEN);
    }

    /* 金币 */
    game.coin.radius = 6;
    game.coin.obj = lv_obj_create(game.container);
    lv_obj_set_size(game.coin.obj,
        game.coin.radius * 2, game.coin.radius * 2);
    lv_obj_set_style_radius(game.coin.obj,
                            LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(game.coin.obj,
        lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_border_width(game.coin.obj, 0, 0);

    /* 道具掉落物（预创建，初始隐藏） */
    game.powerup.radius = 8;
    game.powerup.obj = lv_obj_create(game.container);
    lv_obj_set_size(game.powerup.obj,
        game.powerup.radius * 2,
        game.powerup.radius * 2);
    lv_obj_set_style_radius(game.powerup.obj,
                            LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(
        game.powerup.obj, 2, 0);
    lv_obj_set_style_border_color(
        game.powerup.obj,
        lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(game.powerup.obj,
                    LV_OBJ_FLAG_HIDDEN);

    /* 敌人池 */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        game.enemies[i].radius = 7;
        game.enemies[i].obj =
            lv_obj_create(game.container);
        lv_obj_set_size(game.enemies[i].obj,
            game.enemies[i].radius * 2,
            game.enemies[i].radius * 2);
        lv_obj_set_style_radius(
            game.enemies[i].obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(
            game.enemies[i].obj,
            lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_border_width(
            game.enemies[i].obj, 0, 0);
        lv_obj_add_flag(game.enemies[i].obj,
                        LV_OBJ_FLAG_HIDDEN);
    }

    /* Game Over 提示 */
    game.over_label = lv_label_create(game.container);
    lv_label_set_text(game.over_label,
        "GAME OVER\nPress ENTER to Restart");
    lv_obj_set_style_text_align(game.over_label,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(game.over_label,
        lv_color_hex(0xFF4444), 0);
    lv_obj_center(game.over_label);
    lv_obj_add_flag(game.over_label,
                    LV_OBJ_FLAG_HIDDEN);

    prv_unlock_lvgl();

    Game_Reset();
    game.is_running = true;

    /* ========== 主循环 ========== */
    while (game.is_running) {

        if (Input_Is_Key_Held(SYS_KEY_BACK)) {
            game.is_running = false;
            break;
        }

        if (game.phase == GAME_PHASE_PLAYING) {

            float friction = 0.94f;
            float acc_mul = 0.15f;

            if (Input_Is_Key_Held(SYS_KEY_ENTER)) {
                friction = 0.98f;
                acc_mul = 0.35f;
            }

            float acc_x = MyIMU.angle[1] * acc_mul;
            float acc_y = MyIMU.angle[0] * acc_mul;

            game.player.vx =
                (game.player.vx + acc_x) * friction;
            game.player.vy =
                (game.player.vy + acc_y) * friction;
            game.player.x += game.player.vx;
            game.player.y += game.player.vy;

            if (game.player.x < game.player.radius) {
                game.player.x = game.player.radius;
                game.player.vx *= -0.5f;
            }
            if (game.player.x >
                SCREEN_W - game.player.radius) {
                game.player.x =
                    SCREEN_W - game.player.radius;
                game.player.vx *= -0.5f;
            }
            if (game.player.y < game.player.radius) {
                game.player.y = game.player.radius;
                game.player.vy *= -0.5f;
            }
            if (game.player.y >
                SCREEN_H - game.player.radius) {
                game.player.y =
                    SCREEN_H - game.player.radius;
                game.player.vy *= -0.5f;
            }

            /* 记录拖尾轨迹 */
            game.trail_x[game.trail_head] =
                game.player.x;
            game.trail_y[game.trail_head] =
                game.player.y;
            game.trail_head =
                (game.trail_head + 1) % TRAIL_LENGTH;

            /* 敌人移动（时缓下减速） */
            float enemy_speed = 1.0f;
            if (game.active.slow_frames > 0) {
                enemy_speed = 0.35f;
            }

            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (!game.enemies[i].active) continue;

                game.enemies[i].x +=
                    game.enemies[i].vx * enemy_speed;
                game.enemies[i].y +=
                    game.enemies[i].vy * enemy_speed;

                if (game.enemies[i].x <
                        game.enemies[i].radius ||
                    game.enemies[i].x >
                        SCREEN_W -
                        game.enemies[i].radius)
                    game.enemies[i].vx *= -1.0f;
                if (game.enemies[i].y <
                        game.enemies[i].radius ||
                    game.enemies[i].y >
                        SCREEN_H -
                        game.enemies[i].radius)
                    game.enemies[i].vy *= -1.0f;

                /* 护盾免疫碰撞 */
                bool blocked = false;
                if (game.active.shield_frames > 0) {
                    Entity_t shield_a = game.player;
                    shield_a.radius += 8;
                    if (Check_Collision(
                            &shield_a,
                            &game.enemies[i])) {
                        game.enemies[i].active = false;
                        blocked = true;
                    }
                }

                if (!blocked &&
                    Check_Collision(
                        &game.player,
                        &game.enemies[i])) {
                    game.combo_count = 0;
                    game.phase = GAME_PHASE_GAMEOVER;
                }
            }

            /* 吸铁石：金币飞向玩家 */
            if (game.active.magnet_frames > 0) {
                float dx = game.player.x - game.coin.x;
                float dy = game.player.y - game.coin.y;
                game.coin.x += dx * 0.12f;
                game.coin.y += dy * 0.12f;
            }

            /* 吃金币 */
            if (Check_Collision(&game.player,
                                &game.coin)) {
                game.combo_count++;
                int combo_mul =
                    (game.combo_count / 3) + 1;
                if (combo_mul > 3) combo_mul = 3;
                game.score += 10 * combo_mul;

                game.popup_value = 10 * combo_mul;
                game.popup_life = 25;

                Spawn_Entity_Randomly(&game.coin);

                int target = game.score / 30;
                if (target > MAX_ENEMIES)
                    target = MAX_ENEMIES;

                for (int i = 0; i < target; i++) {
                    if (!game.enemies[i].active) {
                        game.enemies[i].active = true;
                        Spawn_Entity_Randomly(
                            &game.enemies[i]);
                        game.enemies[i].vx =
                            ((rand() % 50) / 10.0f)
                            - 2.5f;
                        game.enemies[i].vy =
                            ((rand() % 50) / 10.0f)
                            - 2.5f;
                    }
                }

                /* 概率掉落道具 */
                if (!game.powerup.active &&
                    (rand() % 100) <
                        POWERUP_DROP_CHANCE) {
                    Spawn_PowerUp();
                }
            }

            /* 拾取道具 */
            if (game.powerup.active) {
                float dx = game.player.x - game.powerup.x;
                float dy = game.player.y - game.powerup.y;
                float dist = dx * dx + dy * dy;
                float pick_r = game.player.radius
                             + game.powerup.radius;
                if (dist < pick_r * pick_r) {
                    Activate_PowerUp(game.powerup.type);
                    game.powerup.active = false;
                }
            }

            /* 道具倒计时 */
            if (game.active.shield_frames > 0)
                game.active.shield_frames--;
            if (game.active.slow_frames > 0)
                game.active.slow_frames--;
            if (game.active.magnet_frames > 0)
                game.active.magnet_frames--;

        } else if (game.phase == GAME_PHASE_GAMEOVER) {
            if (Input_Is_Key_Held(SYS_KEY_ENTER)) {
                game.phase = GAME_PHASE_PLAYING;
                Game_Reset();
            }
        }

        /* ---------- 加锁渲染 ---------- */
        if (!prv_lock_lvgl()) {
            osDelay(5);
            continue;
        }

        if (game.phase == GAME_PHASE_PLAYING) {

            lv_obj_add_flag(game.over_label,
                            LV_OBJ_FLAG_HIDDEN);

            lv_label_set_text_fmt(game.score_label,
                "SCORE: %d", game.score);

            /* Combo */
            if (game.combo_count >= 3) {
                lv_label_set_text_fmt(
                    game.combo_label, "COMBO x%d!",
                    1 + game.combo_count / 3);
                if (lv_obj_has_flag(game.combo_label,
                        LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_remove_flag(
                        game.combo_label,
                        LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                if (!lv_obj_has_flag(
                        game.combo_label,
                        LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(
                        game.combo_label,
                        LV_OBJ_FLAG_HIDDEN);
                }
            }

            /* 得分弹出 */
            if (game.popup_life > 0) {
                if (game.popup_life == 25) {
                    lv_label_set_text_fmt(
                        game.popup_label, "+%d",
                        game.popup_value);
                    lv_obj_remove_flag(
                        game.popup_label,
                        LV_OBJ_FLAG_HIDDEN);
                }
                game.popup_life--;
                int32_t py = (int32_t)(
                    game.coin.y - game.coin.radius)
                    - (25 - game.popup_life) * 2;
                lv_obj_set_pos(game.popup_label,
                    (int32_t)(
                        game.coin.x - game.coin.radius),
                    py);
                lv_obj_set_style_text_opa(
                    game.popup_label,
                    (uint8_t)(game.popup_life
                        * 255 / 25), 0);
            } else {
                if (!lv_obj_has_flag(
                        game.popup_label,
                        LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(
                        game.popup_label,
                        LV_OBJ_FLAG_HIDDEN);
                }
            }

            /* 玩家呼吸 */
            game.pulse_frame++;
            int32_t pe = 0;
            if ((game.pulse_frame / 10) % 2 == 0)
                pe = 1;

            /* 护盾视觉：玩家边框蓝色 */
            if (game.active.shield_frames > 0) {
                lv_obj_set_style_border_width(
                    game.player.obj, 3, 0);
                lv_obj_set_style_border_color(
                    game.player.obj,
                    lv_color_hex(0x4488FF), 0);
            } else {
                lv_obj_set_style_border_width(
                    game.player.obj, 0, 0);
            }

            lv_obj_set_size(game.player.obj,
                (int32_t)(game.player.radius * 2 + pe),
                (int32_t)(game.player.radius * 2 + pe));
            lv_obj_set_pos(game.player.obj,
                (int32_t)(game.player.x
                    - game.player.radius),
                (int32_t)(game.player.y
                    - game.player.radius));

            /* 拖尾（从旧到新依次变亮） */
            for (int i = 0; i < TRAIL_LENGTH; i++) {
                int idx = (game.trail_head + i)
                          % TRAIL_LENGTH;
                uint8_t opa = (uint8_t)(
                    (i + 1) * 80 / TRAIL_LENGTH);
                if (opa < 15) opa = 15;

                if (i < TRAIL_LENGTH - 2) {
                    lv_obj_set_style_bg_opa(
                        game.trail_dots[idx],
                        opa, 0);
                    lv_obj_remove_flag(
                        game.trail_dots[idx],
                        LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_pos(
                        game.trail_dots[idx],
                        (int32_t)(
                            game.trail_x[idx]
                            - game.player.radius),
                        (int32_t)(
                            game.trail_y[idx]
                            - game.player.radius));
                } else {
                    lv_obj_add_flag(
                        game.trail_dots[idx],
                        LV_OBJ_FLAG_HIDDEN);
                }
            }

            /* 金币 */
            lv_obj_set_pos(game.coin.obj,
                (int32_t)(game.coin.x - game.coin.radius),
                (int32_t)(game.coin.y - game.coin.radius));

            /* 道具掉落物 */
            if (game.powerup.active) {
                lv_obj_set_style_bg_color(
                    game.powerup.obj,
                    lv_color_hex(
                        pu_color(game.powerup.type)),
                    0);
                lv_obj_set_style_border_color(
                    game.powerup.obj,
                    lv_color_hex(0xFFFFFF), 0);
                if (lv_obj_has_flag(game.powerup.obj,
                        LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_remove_flag(
                        game.powerup.obj,
                        LV_OBJ_FLAG_HIDDEN);
                }
                lv_obj_set_pos(game.powerup.obj,
                    (int32_t)(game.powerup.x
                        - game.powerup.radius),
                    (int32_t)(game.powerup.y
                        - game.powerup.radius));
            } else {
                if (!lv_obj_has_flag(
                        game.powerup.obj,
                        LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(
                        game.powerup.obj,
                        LV_OBJ_FLAG_HIDDEN);
                }
            }

            /* 敌人（时缓下变紫） */
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (game.enemies[i].active) {
                    lv_obj_remove_flag(
                        game.enemies[i].obj,
                        LV_OBJ_FLAG_HIDDEN);
                    if (game.active.slow_frames > 0) {
                        lv_obj_set_style_bg_color(
                            game.enemies[i].obj,
                            lv_color_hex(0xAA44FF), 0);
                    } else {
                        lv_obj_set_style_bg_color(
                            game.enemies[i].obj,
                            lv_color_hex(0xFF0000), 0);
                    }
                    lv_obj_set_pos(
                        game.enemies[i].obj,
                        (int32_t)(game.enemies[i].x
                            - game.enemies[i].radius),
                        (int32_t)(game.enemies[i].y
                            - game.enemies[i].radius));
                } else {
                    lv_obj_add_flag(
                        game.enemies[i].obj,
                        LV_OBJ_FLAG_HIDDEN);
                }
            }

            /* 道具状态条 */
            char buf[48];
            buf[0] = '\0';
            if (game.active.shield_frames > 0) {
                int sec = game.active.shield_frames / 50;
                snprintf(buf, sizeof(buf),
                    "SHIELD %ds", sec);
            }
            if (game.active.slow_frames > 0) {
                int sec = game.active.slow_frames / 50;
                size_t off = strlen(buf);
                snprintf(buf + off,
                    sizeof(buf) - off,
                    "%sSLOW %ds",
                    (off > 0) ? " " : "", sec);
            }
            if (game.active.magnet_frames > 0) {
                int sec = game.active.magnet_frames / 50;
                size_t off = strlen(buf);
                snprintf(buf + off,
                    sizeof(buf) - off,
                    "%sMAGNET %ds",
                    (off > 0) ? " " : "", sec);
            }
            if (buf[0] != '\0') {
                lv_label_set_text(
                    game.active.status_label, buf);
                lv_obj_remove_flag(
                    game.active.status_label,
                    LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(
                    game.active.status_label,
                    LV_OBJ_FLAG_HIDDEN);
            }

        } else {
            lv_obj_remove_flag(game.over_label,
                               LV_OBJ_FLAG_HIDDEN);
        }

        prv_unlock_lvgl();
        osDelay(20);
    }

    if (prv_lock_lvgl()) {
        if (game.container) {
            lv_obj_del(game.container);
            game.container = NULL;
        }
        prv_unlock_lvgl();
    }

    game_task_handle = NULL;
    osThreadExit();
}

void Game_Start(void)
{
    if (game_task_handle == NULL) {
        const osThreadAttr_t game_attr = {
            .name = "MiniGame",
            .stack_size = 1024 * 8,
            .priority = osPriorityNormal,
        };
        game_task_handle = osThreadNew(
            Game_Entry, NULL, &game_attr);
    }
}

void Game_Stop(void)
{
    game.is_running = false;
}

bool Game_IsRunning(void)
{
    return (game_task_handle != NULL);
}
