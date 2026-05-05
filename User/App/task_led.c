/**
 * @file    task_led.c
 * @brief   LED control task with message-queue commands.
 * @author  Ltttttts
 */

#define LED_QUEUE_DEPTH     (10U)
#define BLINK_INTERVAL_DFT  (500U)
#define BLINK_INTERVAL_MIN  (100U)

#include "task_led.h"
#include "bsp_led.h"

osMessageQueueId_t led_cmd_queue = NULL;

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

