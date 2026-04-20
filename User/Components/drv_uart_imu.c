#include "drv_uart_imu.h"
#include "main.h"
#include "logger.h"

extern UART_HandleTypeDef huart4; // 引入底层句柄

// 定义环形缓冲区 (Ring Buffer)
#define IMU_RX_BUF_SIZE 256
static uint8_t rx_fifo[IMU_RX_BUF_SIZE];
static uint16_t head = 0; // 写指针
static uint16_t tail = 0; // 读指针
static uint8_t rx_byte;   // 临时接收变量

/**
 * @brief 启动底层串口接收
 */
void Drv_IMU_UART_Init(void) {
    // 在启动接收前，强行清除可能存在的溢出错误 (ORE)！
    // 针对 STM32H7 系列，清除标志位的方法：
    __HAL_UART_CLEAR_FLAG(&huart4, UART_CLEAR_OREF);
    
    // 丢弃掉寄存器里可能残留的垃圾死数据
    __HAL_UART_FLUSH_DRREGISTER(&huart4);
    
    // 3. 启动中断接收，并检查返回值！
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
    
    if (status != HAL_OK) {
        LOG_E("UART4", "IMU UART IT Start Failed! Code: %d", status);
    } else {
        LOG_I("UART4", "IMU UART IT Started Successfully.");
    }
}
/**
 * @brief 接管系统的串口中断回调
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart4) {
        // 1. 把数据扔进水池
        rx_fifo[head++] = rx_byte;
        if (head >= IMU_RX_BUF_SIZE) head = 0; // 环形折返
        
        // 2. 重新开启中断
        HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
    }
}

/**
 * @brief 供上层调用的读取接口：把水池里的水抽出来
 */
uint16_t Drv_IMU_Read_Data(uint8_t *out_buf, uint16_t max_len) {
    uint16_t count = 0;
    // 当读指针没追上写指针时，说明有新数据
    while (tail != head && count < max_len) {
        out_buf[count++] = rx_fifo[tail++];
        if (tail >= IMU_RX_BUF_SIZE) tail = 0;
    }
    return count; // 返回实际读到了多少个字节
}



