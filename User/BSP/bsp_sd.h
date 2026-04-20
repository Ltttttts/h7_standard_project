/**
 ******************************************************************************
 * @file    bsp_sd.h
 * @brief   SD卡底层驱动封装头文件 (适配 STM32H7 + SDMMC)
 ******************************************************************************
 */

#ifndef __BSP_SD_H
#define __BSP_SD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/* =======================================================
 * 宏定义区
 * ======================================================= */
// SD卡检测状态 (特别针对无CD引脚的板子)
#define SD_PRESENT               ((uint8_t)0x01)
#define SD_NOT_PRESENT           ((uint8_t)0x00)

// SD卡传输状态
#define SD_TRANSFER_OK           ((uint8_t)0x00)
#define SD_TRANSFER_BUSY         ((uint8_t)0x01)
#define SD_TRANSFER_ERROR        ((uint8_t)0x02)

/* =======================================================
 * API 函数声明
 * ======================================================= */
// 初始化与状态检测
uint8_t BSP_SD_Init(void);
uint8_t BSP_SD_IsDetected(void);
uint8_t BSP_SD_GetCardState(void);
void    BSP_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo);

// 阻塞式读写 (不推荐在 RTOS 中大量使用，会卡死任务)
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout);
uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout);

// DMA 非阻塞式读写 (RTOS + FatFs 强烈推荐)
uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks);
uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks);

// 擦除
uint8_t BSP_SD_Erase(uint32_t StartAddr, uint32_t EndAddr);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SD_H */


