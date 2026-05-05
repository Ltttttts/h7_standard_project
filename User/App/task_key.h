/**
 * @file    task_key.h
 * @brief   Key scan task header.
 * @author  Ltttttts
 */

#ifndef __TASK_KEY_H
#define __TASK_KEY_H

#include "cmsis_os2.h"
#include "bsp_key.h"
#include "key_dev.h"

typedef struct {
    uint8_t       id;
    Key_Event_e   event;
} KeyEventMsg_t;

void Task_Key_Entry(void *argument);

#endif /* __TASK_KEY_H */
