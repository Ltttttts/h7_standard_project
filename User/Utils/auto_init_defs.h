/**
 * @file    auto_init_defs.h
 * @brief   自动初始化注册表 — 在这里登记所有需要自动初始化的模块
 * @author  Ltttttts
 *
 * ============================================================
 *  用法说明
 * ============================================================
 *
 *  每添加一个新模块，只需在此文件加一行：
 *
 *      AUTO_INIT_ENTRY(模块初始化函数名, 初始化级别)
 *
 *  ⚠️ 注意：
 *    1. 初始化函数必须是 void xxx(void) 签名（无返回值、无参数）
 *    2. 初始化函数不能是 static（需要被 auto_init.c 跨文件调用）
 *    3. 级别值越小越先执行，参考 auto_init.h 中的 7 级定义
 *    4. 同级别的多个函数按此文件中的书写顺序执行
 *
 * ============================================================
 *  本文件不包含 #define 或 #include
 * ============================================================
 *
 *  这是 X-Macro 模式的核心文件。整个文件只包含
 *  AUTO_INIT_ENTRY(...) 调用行，不包含任何预处理指令。
 *
 *  它在 auto_init.c 中被 #include 两次：
 *    第一次：展开为 extern 声明（告诉编译器这些函数存在）
 *    第二次：展开为数组元素（构建初始化表）
 *
 *  因此不要在此文件加 #define 或 #include，
 *  否则在第二次展开时会重复定义导致编译错误。
 */

/* ========== Level 1: 板级硬件初始化 ========== */
AUTO_INIT_ENTRY(BSP_LED_Init,          AUTO_INIT_LEVEL_BOARD)
AUTO_INIT_ENTRY(prv_bsp_sd_init,       AUTO_INIT_LEVEL_BOARD)
AUTO_INIT_ENTRY(prv_uart_hardware_init,AUTO_INIT_LEVEL_BOARD)

/* ========== Level 2: 纯软件基础设施 ========== */
AUTO_INIT_ENTRY(Logger_Init,           AUTO_INIT_LEVEL_PREV)

/* ========== Level 3: 设备驱动注册 ========== */
AUTO_INIT_ENTRY(BSP_LCD_Construct,     AUTO_INIT_LEVEL_DEVICE)

/* ========== Level 4: 组件配置 ========== */
AUTO_INIT_ENTRY(prv_lvgl_hardware_init,AUTO_INIT_LEVEL_COMPONENT)
AUTO_INIT_ENTRY(prv_imu_hardware_init, AUTO_INIT_LEVEL_COMPONENT)

/* ========== Level 6: 应用层 — 创建 RTOS 任务 ========== */
AUTO_INIT_ENTRY(prv_create_led_task,   AUTO_INIT_LEVEL_APP)
AUTO_INIT_ENTRY(prv_create_lvgl_task,  AUTO_INIT_LEVEL_APP)
AUTO_INIT_ENTRY(prv_create_key_task,   AUTO_INIT_LEVEL_APP)
AUTO_INIT_ENTRY(prv_create_uart_task,  AUTO_INIT_LEVEL_APP)
AUTO_INIT_ENTRY(prv_create_imu_task,   AUTO_INIT_LEVEL_APP)
