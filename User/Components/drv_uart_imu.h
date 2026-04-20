#ifndef __DRV_UART_IMU_H
#define __DRV_UART_IMU_H

#include <stdint.h>



void Drv_IMU_UART_Init(void);
uint16_t Drv_IMU_Read_Data(uint8_t *out_buf, uint16_t max_len);




#endif

