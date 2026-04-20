#include "key_dev.h"
#include "main.h" 

// 1. 定义一个硬件映射结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} Key_Hardware_Map_t;

// 2. 【表驱动法】把所有的引脚配置写进这个数组里
// 数组下标就是按键 ID (为了直观，我们让 ID 从 1 开始，所以第 0 个空着)
static const Key_Hardware_Map_t Key_Map[] = {
    {NULL, 0},                             // ID: 0 (弃用)
    {KEY_1_GPIO_Port, KEY_1_Pin},          // ID: 1
    {KEY_2_GPIO_Port, KEY_2_Pin},          // ID: 2
    {KEY_3_GPIO_Port, KEY_3_Pin},          // ID: 3
    {KEY_4_GPIO_Port, KEY_4_Pin},          // ID: 4
};

// 3. 唯一的底层读取函数
uint8_t Drv_Key_Read(uint8_t key_id) 
{
    // 防越界保护 (计算数组最大容量)
    if (key_id < 1 || key_id >= (sizeof(Key_Map)/sizeof(Key_Map[0]))) {
        return 0; 
    }
    
    // 查表读取电平
    GPIO_PinState state = HAL_GPIO_ReadPin(Key_Map[key_id].port, Key_Map[key_id].pin);
    
    // 返回逻辑值：按下(低电平)返回1，松开返回0
    return (state == GPIO_PIN_RESET) ? 1 : 0;
}



