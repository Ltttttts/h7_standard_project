#include "task_uart.h"
#include "usart.h"
#include "input_manager.h" 

osMessageQueueId_t uart_rx_queue = NULL;

// 【新增】：定义 DMA 接收缓冲区
#define UART_DMA_BUF_SIZE 64
// H7 强提醒：DMA 缓冲区最好 32 字节对齐，以防 Cache 污染
uint8_t uart_dma_rx_buf[UART_DMA_BUF_SIZE] __attribute__((aligned(32))); 




// 【核心改变】：专门处理 (DMA接收一半空闲) 或 (DMA缓冲区满) 的事件回调
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        
        // 【STM32H7 专属排雷】：无效化 D-Cache
        // 因为 DMA 是直接把数据写进物理内存的，而 CPU 可能还在看 Cache 里的旧数据。
        // 这行代码强制让 CPU 去内存里重新“进货”，保证数据新鲜。
        SCB_InvalidateDCache_by_Addr((uint32_t *)uart_dma_rx_buf, ((Size + 31) / 32) * 32);

        if (uart_rx_queue != NULL) {
            // 收到多少个字节 (Size)，就循环往队列里塞多少次
            for (uint16_t i = 0; i < Size; i++) {
                osMessageQueuePut(uart_rx_queue, &uart_dma_rx_buf[i], 0, 0);
            }
        }
        
        // 数据提货完毕，重新挂起 DMA 等待下一波数据
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_dma_rx_buf, UART_DMA_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
    }
}


void Task_UART_Entry(void *argument) 
{
    // 队列深度稍微给大点，因为现在是一次性塞进来一批数据
    uart_rx_queue = osMessageQueueNew(128, sizeof(uint8_t), NULL);
    
    // 【核心改变】：首次启动 DMA + 空闲中断接收
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_dma_rx_buf, UART_DMA_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    uint8_t raw_byte;
    while (1) {
        // 阻塞等待：这里的逻辑完全不变！依然是挨个从队列里拿字节
        if (osMessageQueueGet(uart_rx_queue, &raw_byte, NULL, osWaitForever) == osOK) {
            
            InputEventMsg_t msg = {0};
            msg.event = KEY_EVENT_CLICK; 
            msg.key_code = SYS_KEY_NONE;
            msg.source = INPUT_SRC_UART;

            switch (raw_byte) {
                case 'w': case 'W': msg.key_code = SYS_KEY_UP;    break;
                case 's': case 'S': msg.key_code = SYS_KEY_DOWN;  break;
                case 'f': case 'F':msg.key_code = SYS_KEY_ENTER; break;
                case 'q': case 'Q': 
                case 0x1B:          msg.key_code = SYS_KEY_BACK;  break; 
            }

            if (msg.key_code != SYS_KEY_NONE && input_msg_queue != NULL) {
                osMessageQueuePut(input_msg_queue, &msg, 0, 0);
            }
        }
    }
}


