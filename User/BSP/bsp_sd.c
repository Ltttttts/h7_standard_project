/**
 ******************************************************************************
 * @file    bsp_sd.c
 * @brief   SD卡底层驱动封装实现 (适配 STM32H7 + SDMMC)
 ******************************************************************************
 */

#include "bsp_sd.h"
#include "logger.h"
#include "ff.h"

extern SD_HandleTypeDef hsd1;

FATFS SDFatFs;

/* 包装 BSP_SD_Init（返回 uint8_t→void）供 auto_init 调用 */
void prv_bsp_sd_init(void)
{
    (void)BSP_SD_Init();
}

/**
 * @brief  初始化 SD 卡硬件
 * @retval SD_TRANSFER_OK 或 SD_TRANSFER_ERROR
 */
uint8_t BSP_SD_Init(void)
{
    uint8_t sd_state = SD_TRANSFER_OK;

//    // 检查卡是否插入 (走我们的强制破解逻辑)
//    if (BSP_SD_IsDetected() != SD_PRESENT) {
//        return SD_TRANSFER_ERROR;
//    }

    // 此时硬件外设应该已经在 main() 里的 MX_SDMMC1_SD_Init() 初始化过了
    // 如果卡处于挂起状态，尝试恢复
    if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_ERROR) {
        // 尝试重新初始化
		LOG_I("SD","restart");
        if (HAL_SD_Init(&hsd1) != HAL_OK) {
            sd_state = SD_TRANSFER_ERROR;
        }
    }

    return sd_state;
}

/**
 * @brief  检测 SD 卡是否插入
 * @note   【破解逻辑】LXB723ZG-P1 板子无检测引脚，强行返回插入状态
 * @retval SD_PRESENT 或 SD_NOT_PRESENT
 */
uint8_t BSP_SD_IsDetected(void)
{
    // 强制欺骗底层，告诉它卡永远在线！
    return SD_PRESENT;
}

/**
 * @brief  获取卡当前状态
 * @retval SD_TRANSFER_OK / SD_TRANSFER_BUSY / SD_TRANSFER_ERROR
 */
uint8_t BSP_SD_GetCardState(void)
{
    if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) {
        return SD_TRANSFER_OK;
    } else {
        return SD_TRANSFER_BUSY;
    }
}

/**
 * @brief  获取卡信息 (容量、块大小等)
 */
void BSP_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo)
{
    HAL_SD_GetCardInfo(&hsd1, CardInfo);
}

/* =======================================================
 * 阻塞式读写接口
 * ======================================================= */
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    if (HAL_SD_ReadBlocks(&hsd1, (uint8_t *)pData, ReadAddr, NumOfBlocks, Timeout) == HAL_OK) {
        while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {} // 等待卡就绪
        return SD_TRANSFER_OK;
    }
    return SD_TRANSFER_ERROR;
}

uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    if (HAL_SD_WriteBlocks(&hsd1, (uint8_t *)pData, WriteAddr, NumOfBlocks, Timeout) == HAL_OK) {
        while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER) {} // 等待烧写完毕
        return SD_TRANSFER_OK;
    }
    return SD_TRANSFER_ERROR;
}

/* =======================================================
 * DMA 读写接口 (高效模式)
 * ======================================================= */
uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks)
{
    if (HAL_SD_ReadBlocks_DMA(&hsd1, (uint8_t *)pData, ReadAddr, NumOfBlocks) == HAL_OK) {
        return SD_TRANSFER_OK;
    }
    return SD_TRANSFER_ERROR;
}

uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks)
{
    if (HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t *)pData, WriteAddr, NumOfBlocks) == HAL_OK) {
        return SD_TRANSFER_OK;
    }
    return SD_TRANSFER_ERROR;
}

/* =======================================================
 * DMA 传输完成回调函数处理
 * FatFs 在使用 DMA 时，需要靠这几个回调函数来释放信号量 (RTOS)
 * ======================================================= */
// 注意：如果 CubeMX 已经在其他地方（比如 sd_diskio.c 或 stm32h7xx_it.c）
// 生成了这两个函数，你需要把它们注释掉，或者保留那边的，删掉这里的。
/*
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    // 可以在这里发送 FreeRTOS 信号量，通知 FatFs 写入完毕
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    // 可以在这里发送 FreeRTOS 信号量，通知 FatFs 读取完毕
}
*/

