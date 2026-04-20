#include "bsp_lcd.h"
#include "logger.h"

/* 实例化全局屏幕对象 */
LCD_Device_t Screen;

const osMutexAttr_t lcdMutex_attributes = { .name = "lcdMutex" };

/* ========================================================= */
/* 私有方法实现 (Private Methods)          */
/* ========================================================= */

static void Method_Init(LCD_Device_t *self) {
    // 1. 初始化属于自己的互斥锁
    self->Lock = osMutexNew(&lcdMutex_attributes);
    
    // 2. 调用底层的硬件初始化
    SPI_LCD_Init(); 
    
    // 3. 内部上锁配置默认属性
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_SetDirection(Direction_V);
    LCD_SetAsciiFont(&ASCII_Font20);
    self->Width = 240;  // 假设宽240
    self->Height = 240; // 假设高240
    osMutexRelease(self->Lock);
}

static void Method_SetColor(LCD_Device_t *self, uint32_t text_color, uint32_t back_color) {
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_SetColor(text_color);
    LCD_SetBackColor(back_color);
    osMutexRelease(self->Lock);
}

static void Method_Clear(LCD_Device_t *self) {
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_Clear();
    osMutexRelease(self->Lock);
}

static void Method_ShowStr(LCD_Device_t *self, uint16_t x, uint16_t y, char *str) {
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_DisplayString(x, y, str); // 调用底层组件发数据
    osMutexRelease(self->Lock);
}

static void Method_ShowNum(LCD_Device_t *self, uint16_t x, uint16_t y, int32_t num, uint8_t len) {
    osMutexAcquire(self->Lock, osWaitForever);
    LCD_DisplayNumber(x, y, num, len);
    osMutexRelease(self->Lock);
}

static void Method_SetBacklight(LCD_Device_t *self, uint8_t state) {
    if(state) LCD_Backlight_ON
    else      LCD_Backlight_OFF;
}

static void Method_DrawBitmap(LCD_Device_t *self, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t *data) {
    osMutexAcquire(self->Lock, osWaitForever); // 自动上锁保护 SPI 总线
    
    // 调用之前你底层驱动里的批量复制函数 (注意参数：起点和宽高)
    LCD_CopyBuffer(x, y, w, h, data); 
    
    osMutexRelease(self->Lock);                // 自动解锁
}


/* ========================================================= */
/* 类的构造函数 (Constructor)              */
/* ========================================================= */
/**
 * @brief  系统启动时调用，将方法挂载到对象上
 */
void BSP_LCD_Construct(void) {
    // 挂载方法 (函数指针赋值)
    Screen.Init         = Method_Init;
    Screen.SetColor     = Method_SetColor;
    Screen.Clear        = Method_Clear;
    Screen.ShowStr      = Method_ShowStr;
    Screen.ShowNum      = Method_ShowNum;
    Screen.SetBacklight = Method_SetBacklight;
	Screen.DrawBitmap   = Method_DrawBitmap;
    
    LOG_I("BSP_LCD", "LCD Object Constructed.");
}


