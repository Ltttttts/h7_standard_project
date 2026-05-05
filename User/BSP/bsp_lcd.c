/**
 * @file    bsp_lcd.c
 * @brief   LCD device object (OOP wrapper for LCD driver).
 * @author  Ltttttts
 */

#include "bsp_lcd.h"
#include "logger.h"

/* 显示参数 */
#define LCD_WIDTH_DEFAULT   (240U)
#define LCD_HEIGHT_DEFAULT  (240U)

/* 实例化全局屏幕对象 */
LCD_Device_t Screen;

static const osMutexAttr_t s_lcd_mutex_attr = {
    .name = "lcdMutex"
};

static void method_init(LCD_Device_t *self)
{
    self->Lock = osMutexNew(&s_lcd_mutex_attr);
    SPI_LCD_Init();

    osMutexAcquire(self->Lock, osWaitForever);
    LCD_SetDirection(Direction_V);
    LCD_SetAsciiFont(&ASCII_Font20);
    self->Width  = LCD_WIDTH_DEFAULT;
    self->Height = LCD_HEIGHT_DEFAULT;
    osMutexRelease(self->Lock);
}

static void method_set_color(LCD_Device_t *self,
                             uint32_t text_color,
                             uint32_t back_color)
{
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_SetColor(text_color);
    LCD_SetBackColor(back_color);
    osMutexRelease(self->Lock);
}

static void method_clear(LCD_Device_t *self)
{
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_Clear();
    osMutexRelease(self->Lock);
}

static void method_show_str(LCD_Device_t *self,
                            uint16_t x, uint16_t y,
                            const char *str)
{
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_DisplayString(x, y, (char *)str);
    osMutexRelease(self->Lock);
}

static void method_show_num(LCD_Device_t *self,
                            uint16_t x, uint16_t y,
                            int32_t num, uint8_t len)
{
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_DisplayNumber(x, y, num, len);
    osMutexRelease(self->Lock);
}

static void method_set_backlight(
    LCD_Device_t *self, uint8_t state)
{
    (void)self;
    if (state) {
        LCD_Backlight_ON;
    } else {
        LCD_Backlight_OFF;
    }
}

static void method_draw_bitmap(
    LCD_Device_t *self,
    uint16_t x, uint16_t y,
    uint16_t w, uint16_t h,
    uint16_t *data)
{
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_CopyBuffer(x, y, w, h, data);
    osMutexRelease(self->Lock);
}

void BSP_LCD_Construct(void)
{
    Screen.Init         = method_init;
    Screen.SetColor     = method_set_color;
    Screen.Clear        = method_clear;
    Screen.ShowStr      = method_show_str;
    Screen.ShowNum      = method_show_num;
    Screen.SetBacklight = method_set_backlight;
    Screen.DrawBitmap   = method_draw_bitmap;

    LOG_I("BSP_LCD", "LCD Object Constructed.");
}


