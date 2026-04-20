#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_spi_154.h" // 底层组件
#include "cmsis_os2.h"

/* 1. 定义 LCD 设备类 (结构体) */
typedef struct _LCD_Device {
    /* ----- 属性 (Attributes) ----- */
    uint16_t Width;
    uint16_t Height;
    osMutexId_t Lock;  // 对象的私有互斥锁，对应用层隐藏

    /* ----- 方法 (Methods) ----- */
    // 初始化
    void (*Init)        (struct _LCD_Device *self);
    // 属性设置
    void (*SetColor)    (struct _LCD_Device *self, uint32_t text_color, uint32_t back_color);
    void (*SetBacklight)(struct _LCD_Device *self, uint8_t state);
    // 行为动作
    void (*Clear)       (struct _LCD_Device *self);
    void (*ShowStr)     (struct _LCD_Device *self, uint16_t x, uint16_t y, char *str);
    void (*ShowNum)     (struct _LCD_Device *self, uint16_t x, uint16_t y, int32_t num, uint8_t len);
       
    // 专门给 LVGL 等图形库提供的大块显存推送接口
    void (*DrawBitmap)  (struct _LCD_Device *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data);
	
} LCD_Device_t;

/* 2. 对外暴露一个实体对象 */
extern LCD_Device_t Screen;

/* 3. 构造函数声明 */
void BSP_LCD_Construct(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LCD_H */

