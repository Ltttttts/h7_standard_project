/**
 * @file    task_imu.h
 * @brief   IMU data collection task header.
 * @author  Ltttttts
 */

#ifndef __TASK_IMU_H
#define __TASK_IMU_H

#include "bsp_jy61p.h"

extern JY61P_t MyIMU;

void Task_IMU_Entry(void *argument);
void prv_imu_hardware_init(void);

#endif /* __TASK_IMU_H */
