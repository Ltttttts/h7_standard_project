#ifndef __DRV_KEY_H
#define __DRV_KEY_H
#include <stdint.h>

// 对外暴露唯一一个接口，传入按键 ID 即可读取
uint8_t Drv_Key_Read(uint8_t key_id);





#endif



