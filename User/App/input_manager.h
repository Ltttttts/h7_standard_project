/**
 * @file    input_manager.h
 * @brief   Unified input abstraction for buttons and UART.
 * @author  Ltttttts
 */

#ifndef __INPUT_MANAGER_H
#define __INPUT_MANAGER_H

#include "cmsis_os2.h"
#include "bsp_key.h"
#include <stdbool.h>

typedef enum {
    SYS_KEY_NONE = 0,
    SYS_KEY_UP,
    SYS_KEY_DOWN,
    SYS_KEY_ENTER,
    SYS_KEY_BACK
} SysKey_e;

#define INPUT_SRC_HW_KEY    (0U)
#define INPUT_SRC_UART      (1U)

typedef struct {
    SysKey_e     key_code;
    Key_Event_e  event;
    uint8_t      source;
} InputEventMsg_t;

extern osMessageQueueId_t input_msg_queue;

bool Input_Is_Key_Held(SysKey_e key_code);

#endif /* __INPUT_MANAGER_H */
