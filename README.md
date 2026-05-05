# h7_standard_project

基于 STM32H723ZGT6 的嵌入式 GUI 控制台，融合 **LVGL 图形框架**、**FreeRTOS 实时内核**、**面向对象 C 设计**与**安全关键编码规范**。

## 架构亮点

### 面向对象 C 设计（OOP in C）

尽管使用纯 C 语言，项目全程贯彻面向对象设计思想：

- **LCD 设备对象** (`LCD_Device_t`)：通过函数指针表（vtable）+ 不透明指针封装硬件差异，`Init` / `Clear` / `DrawBitmap` / `SetBacklight` 等操作全部通过对象方法调用
- **LED 设备对象** (`LED_Device_t`)：`On` / `Off` / `Toggle` 接口统一，应用层无需关心 GPIO 寄存器
- **按键抽象层** (`Key_t`)：每个按键是一个独立对象，内置状态机（消抖 / 短按 / 长按），通过 `Key_Init` + `Key_Update` + `Key_GetEvent` 三接口完整封装生命周期
- **输入管理器** (`InputManager`)：将物理按键和串口命令统一为 `InputEventMsg_t` 消息，上层仅消费 `SYS_KEY_UP/DOWN/ENTER/BACK` 虚拟键码，彻底解耦输入源

### 自动初始化机制（Linux 风格 Initcalls）

参考 Linux 内核的 `initcall` 机制，使用 **链接器段 + 宏注册** 实现模块初始化的自动编排：

```c
// 在任何 .c 文件中注册，无需手动调用
AUTO_INIT_EXPORT(BSP_LED_Init, AUTO_INIT_LEVEL_BOARD);
AUTO_INIT_EXPORT(Logger_Init, AUTO_INIT_LEVEL_PREV);
AUTO_INIT_EXPORT(prv_create_lvgl_task, AUTO_INIT_LEVEL_APP);
```

支持 7 个初始化级别（Board → Prev → Device → Component → Env → App → Ready），运行时按优先级升序自动执行。`StartTask_Entry` 仅需一行 `auto_init_run()`。

### 层次化架构与单一职责

```
应用层（Actor + 业务逻辑）
    ↓ 仅调用 API
中间件层（LVGL、FATFS 等）
    ↓ 调用驱动 API
驱动层（OOP 设备驱动：LCD、LED、按键、IMU、SD）
    ↓ 仅调用 BSP 层
板级层（BSP 封装：bsp_lcd、bsp_led、bsp_sd 等）
    ↓ 调用 HAL
芯片层（STM32 HAL 库）
```

每层只依赖下一层的抽象接口，禁止跨层调用。

### RTOS 任务设计

基于 FreeRTOS + CMSIS-RTOS2，每个功能模块独立为 RTOS 任务，通过**消息队列**通信，无共享状态：

| 任务 | 职责 | 通信方式 |
|------|------|---------|
| `LED_Task` | 控制状态 LED（常亮/常灭/闪烁） | 接收 `LedCmdMsg_t` 消息队列 |
| `Key_Task` | 扫描物理按键，映射为虚拟键码 | 投递 `InputEventMsg_t` 到全局队列 |
| `LVGL_Task` | LVGL 渲染循环 + UI 事件路由 | 读取输入队列，调用 LVGL API |
| `IMU_Task` | 串口 JY61P 数据采集与解析 | 串口中断 + DMA 双缓冲 |
| `UART_Task` | 串口指令接收与转发 | 与 `InputManager` 集成 |

### 编码质量保障

项目内建 **嵌入式 C 编码规范**（`.codebuddy/rules/embedded-c-coding.mdc`），强制遵守：

- **80 行函数 / 4 层嵌套 / 80 列行宽** 严格限制
- **`static` + `const` 正确性**：非公共符号强制 `static`，只读指针强制 `const`
- **防御性编程**：断言检查内部契约、运行时校验外部输入、错误码传播至调用链
- **魔法数字禁用**：所有常量定义为宏
- **表驱动设计**：按键映射、IMU 数据循环等用表替代 if-else 链
- **编辑后安全检查清单**：12 阶段审查（编译 → 逻辑 → 内存 → 线程 → 可重入 → 栈 → 资源 → 硬件 → 鲁棒性 → 质量 → SOLID → 影响分析）
- **标准 `@author` + 头文件保护**：所有文件统一头文件保护宏和 `@author` 标注

## 目录结构

```
User/
├── App/           # 应用任务层（app_main / task_led / task_lvgl / task_key 等）
├── BSP/           # 板级支持包（bsp_lcd / bsp_led / bsp_key / bsp_sd / bsp_jy61p）
├── Components/    # 组件驱动（lcd_spi_154 / key_dev / led_dev / drv_uart_imu）
└── Utils/         # 通用工具（logger / auto_init）
```

## 技术栈

| 组件 | 说明 |
|------|------|
| MCU | STM32H723ZGT6 (Cortex-M7, 550MHz) |
| RTOS | FreeRTOS + CMSIS-RTOS2 |
| GUI | LVGL 9.x |
| 文件系统 | FatFS + SDMMC |
| 开发环境 | MDK-ARM (ARMCC V5) |
