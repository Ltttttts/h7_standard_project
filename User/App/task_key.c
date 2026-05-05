/**
 * @file    task_key.c
 * @brief   Key scan task. Maps hardware keys to virtual keys.
 * @author  Ltttttts
 */

#include "task_key.h"
#include "input_manager.h"

#define KEY_NUM             (4U)
#define KEY_QUEUE_DEPTH     (10U)
#define KEY_SCAN_INTERVAL   (10U)
#define LONG_PRESS_THRESH   (1000U)

/* Ó²¼þ°´¼üIDÓ³Éä */
#define HW_KEY_UP           (1U)
#define HW_KEY_DOWN         (2U)
#define HW_KEY_ENTER        (3U)
#define HW_KEY_BACK         (4U)

osMessageQueueId_t input_msg_queue = NULL;

static Key_t s_keys[KEY_NUM];

bool Input_Is_Key_Held(SysKey_e key_code)
{
    switch (key_code) {
    case SYS_KEY_UP:
        return (Drv_Key_Read(HW_KEY_UP) == 1);
    case SYS_KEY_DOWN:
        return (Drv_Key_Read(HW_KEY_DOWN) == 1);
    case SYS_KEY_ENTER:
        return (Drv_Key_Read(HW_KEY_ENTER) == 1);
    case SYS_KEY_BACK:
        return (Drv_Key_Read(HW_KEY_BACK) == 1);
    default:
        return false;
    }
}

void Task_Key_Entry(void *argument)
{
    (void)argument;

    input_msg_queue = osMessageQueueNew(
        KEY_QUEUE_DEPTH, sizeof(InputEventMsg_t), NULL);

    for (uint32_t i = 0; i < KEY_NUM; i++) {
        Key_Init(&s_keys[i], i + 1, Drv_Key_Read,
                 LONG_PRESS_THRESH);
    }

    while (1) {
        for (uint32_t i = 0; i < KEY_NUM; i++) {
            Key_Update(&s_keys[i], KEY_SCAN_INTERVAL);
            Key_Event_e event = Key_GetEvent(&s_keys[i]);

            if (event == KEY_EVENT_NONE) {
                continue;
            }

            InputEventMsg_t msg = {0};
            msg.event = event;
            msg.source = INPUT_SRC_HW_KEY;

            switch (s_keys[i].id) {
            case HW_KEY_UP:
                msg.key_code = SYS_KEY_UP;    break;
            case HW_KEY_DOWN:
                msg.key_code = SYS_KEY_DOWN;  break;
            case HW_KEY_ENTER:
                msg.key_code = SYS_KEY_ENTER; break;
            case HW_KEY_BACK:
                msg.key_code = SYS_KEY_BACK;  break;
            default:
                msg.key_code = SYS_KEY_NONE;  break;
            }

            if (msg.key_code != SYS_KEY_NONE) {
                osMessageQueuePut(input_msg_queue,
                                  &msg, 0, 0);
            }
        }
        osDelay(KEY_SCAN_INTERVAL);
    }
}


