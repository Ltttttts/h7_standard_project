/**
 * @file    task_imu.c
 * @brief   IMU data collection task.
 * @author  Ltttttts
 */

#include "bsp_jy61p.h"
#include "drv_uart_imu.h"
#include "logger.h"
#include "cmsis_os2.h"

JY61P_t MyIMU;

void prv_imu_hardware_init(void)
{
    Drv_IMU_UART_Init();
    JY61P_Init(&MyIMU, 1, Drv_IMU_Read_Data);
}

void Task_IMU_Entry(void *argument)
{
    (void)argument;

    while (1) {
        JY61P_Update(&MyIMU);
        osDelay(50);
    }
}
