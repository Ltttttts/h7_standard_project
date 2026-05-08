/**
 * @file    task_uart.c
 * @brief   UART command input task with DMA + Idle Line.
 * @author  Ltttttts
 */

#include "task_uart.h"
#include "usart.h"
#include "input_manager.h"

#define UART_DMA_BUF_SIZE   (64U)
#define UART_QUEUE_DEPTH    (128U)

osMessageQueueId_t uart_rx_queue = NULL;
static uint8_t uart_dma_rx_buf[UART_DMA_BUF_SIZE]
    __attribute__((aligned(32)));

void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1) {
        SCB_InvalidateDCache_by_Addr(
            (uint32_t *)uart_dma_rx_buf,
            ((Size + 31) / 32) * 32);

        if (uart_rx_queue != NULL) {
            for (uint16_t i = 0; i < Size; i++) {
                osMessageQueuePut(uart_rx_queue,
                    &uart_dma_rx_buf[i], 0, 0);
            }
        }

        HAL_UARTEx_ReceiveToIdle_DMA(
            &huart1, uart_dma_rx_buf,
            UART_DMA_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(
            huart1.hdmarx, DMA_IT_HT);
    }
}

void prv_uart_hardware_init(void)
{
    uart_rx_queue = osMessageQueueNew(
        UART_QUEUE_DEPTH, sizeof(uint8_t), NULL);

    HAL_UARTEx_ReceiveToIdle_DMA(
        &huart1, uart_dma_rx_buf,
        UART_DMA_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(
        huart1.hdmarx, DMA_IT_HT);
}

void Task_UART_Entry(void *argument)
{
    (void)argument;

    uint8_t raw_byte;
    while (1) {
        if (osMessageQueueGet(uart_rx_queue,
                &raw_byte, NULL,
                osWaitForever) == osOK) {

            InputEventMsg_t msg = {0};
            msg.event = KEY_EVENT_CLICK;
            msg.key_code = SYS_KEY_NONE;
            msg.source = INPUT_SRC_UART;

            switch (raw_byte) {
            case 'w': case 'W':
                msg.key_code = SYS_KEY_UP;    break;
            case 's': case 'S':
                msg.key_code = SYS_KEY_DOWN;  break;
            case 'f': case 'F':
                msg.key_code = SYS_KEY_ENTER; break;
            case 'q': case 'Q':
            case 0x1B:
                msg.key_code = SYS_KEY_BACK;  break;
            }

            if (msg.key_code != SYS_KEY_NONE &&
                input_msg_queue != NULL) {
                osMessageQueuePut(input_msg_queue,
                                  &msg, 0, 0);
            }
        }
    }
}
