/**
 * @file    task_led.c
 * @brief   LED 控制任务，通过消息队列接收 UI 命令。
 * @author  Ltttttts
 */

#define LED_QUEUE_DEPTH     (10U)
#define BLINK_INTERVAL_DFT  (500U)
#define BLINK_INTERVAL_MIN  (100U)
#define FLOW_INTERVAL_DFT   (200U)
#define FLOW_INTERVAL_MIN   (50U)

#include "task_led.h"
#include "bsp_led.h"

osMessageQueueId_t led_cmd_queue = NULL;
osMessageQueueId_t flow_led_cmd_queue = NULL;

/* 点亮指定流水灯，并关闭其他流水灯。 */
static void prv_flow_led_set_single(uint8_t index)
{
    BSP_LED_Flow_AllOff();
    if (index < BSP_LED_FLOW_NUM) {
        LedFlow[index].On(&LedFlow[index]);
    }
}

void Task_LED_Entry(void *argument)
{
    (void)argument;

    led_cmd_queue = osMessageQueueNew(
        LED_QUEUE_DEPTH, sizeof(LedCmdMsg_t), NULL);

    LedMode_e current_mode = LED_MODE_BLINK;
    uint32_t blink_interval = BLINK_INTERVAL_DFT;

    while (1) {
        LedCmdMsg_t cmd;
        uint32_t wait = (current_mode == LED_MODE_BLINK)
                        ? blink_interval
                        : osWaitForever;

        osStatus_t res = osMessageQueueGet(
            led_cmd_queue, &cmd, NULL, wait);

        if (res == osOK) {
            current_mode = cmd.mode;
            if (cmd.interval_ms >= BLINK_INTERVAL_MIN) {
                blink_interval = cmd.interval_ms;
            }
        }

        switch (current_mode) {
        case LED_MODE_ON:
            LedStatus.On(&LedStatus);
            break;
        case LED_MODE_OFF:
            LedStatus.Off(&LedStatus);
            break;
        case LED_MODE_BLINK:
            LedStatus.Toggle(&LedStatus);
            break;
        }
    }
}

/*
 * 流水灯任务：
 * - 阻塞等待 UI 命令；
 * - 收到模式后按固定周期刷新 4 个流水灯；
 * - 周期和样式都通过 flow_led_cmd_queue 更新。
 */
void Task_FlowLED_Entry(void *argument)
{
    (void)argument;

    flow_led_cmd_queue = osMessageQueueNew(
        LED_QUEUE_DEPTH, sizeof(FlowLedCmdMsg_t), NULL);

    FlowLedMode_e current_mode = FLOW_LED_MODE_OFF;
    uint32_t interval = FLOW_INTERVAL_DFT;
    uint8_t current_index = 0U;
    int8_t bounce_dir = 1;
    uint8_t all_on = 0U;

    BSP_LED_Flow_AllOff();

    while (1) {
        FlowLedCmdMsg_t cmd;
        uint32_t wait = (current_mode == FLOW_LED_MODE_OFF)
                        ? osWaitForever
                        : interval;

        osStatus_t res = osMessageQueueGet(
            flow_led_cmd_queue, &cmd, NULL, wait);

        if (res == osOK) {
            current_mode = cmd.mode;
            if (cmd.interval_ms >= FLOW_INTERVAL_MIN) {
                interval = cmd.interval_ms;
            }
            current_index = (current_mode == FLOW_LED_MODE_REVERSE)
                            ? (BSP_LED_FLOW_NUM - 1U)
                            : 0U;
            bounce_dir = 1;
            all_on = 0U;
            BSP_LED_Flow_AllOff();
        }

        switch (current_mode) {
        case FLOW_LED_MODE_OFF:
            BSP_LED_Flow_AllOff();
            break;

        case FLOW_LED_MODE_FORWARD:
            prv_flow_led_set_single(current_index);
            current_index++;
            if (current_index >= BSP_LED_FLOW_NUM) {
                current_index = 0U;
            }
            break;

        case FLOW_LED_MODE_REVERSE:
            prv_flow_led_set_single(current_index);
            if (current_index == 0U) {
                current_index = BSP_LED_FLOW_NUM - 1U;
            } else {
                current_index--;
            }
            break;

        case FLOW_LED_MODE_BOUNCE:
            prv_flow_led_set_single(current_index);
            if (current_index == 0U) {
                bounce_dir = 1;
            } else if (current_index >= BSP_LED_FLOW_NUM - 1U) {
                bounce_dir = -1;
            }
            current_index = (uint8_t)((int8_t)current_index + bounce_dir);
            break;

        case FLOW_LED_MODE_ALL_BLINK:
            all_on = !all_on;
            for (uint8_t i = 0U; i < BSP_LED_FLOW_NUM; i++) {
                if (all_on) {
                    LedFlow[i].On(&LedFlow[i]);
                } else {
                    LedFlow[i].Off(&LedFlow[i]);
                }
            }
            break;
        }
    }
}

