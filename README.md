# h7_standard_project

这是一个基于 STM32H7 系列（STM32H723ZGTx）的参考工程，使用 STM32Cube HAL 和 CMSIS-RTOS（FreeRTOS）作为运行时。工程包含常用外设驱动与示例任务：LCD（LVGL）、SD 卡（FATFS）、LED、按键、IMU、串口通信（USART/UART）、SPI、DMA 等。

**主要特性**
- FreeRTOS（CMSIS-RTOS）任务示例：LED、LVGL、KEY、IMU、UART
- FATFS：SD 卡读写支持（见 Target / BSP 实现）
- LVGL：图形界面任务示例（Middlewares/LVGL）
- BSP：LED、LCD、SD、KEY 等板级驱动（User/BSP）
- 外设：USART1 / UART4 (115200 8N1)、SPI6、SDMMC1、DMA、GPIO

**硬件与固件依赖**
- MCU: STM32H723ZGTx（LQFP144）。配置见 [h7_standard_project.ioc](h7_standard_project.ioc)
- STM32Cube FW: STM32Cube FW_H7 V1.12.1（在 .ioc 中声明）
- 开发工具（任选其一）：Keil MDK-ARM（工程文件见 `MDK-ARM/`）、或使用 STM32CubeMX/CubeIDE 生成并使用 GCC/ARM 工具链
- 烧录工具：ST-Link / Keil Flash 等

**快速开始（Keil）**
1. 安装 Keil MDK-ARM 或其它你偏好的 ARM 工具链。
2. （可选）在 STM32CubeMX 中打开 [h7_standard_project.ioc](h7_standard_project.ioc) 并根据需要重新生成代码（CubeMX v6.14.1 是该项目使用的版本）。
3. 用 Keil 打开 `MDK-ARM/h7_standard_project.uvprojx` 并 Build。
4. 使用 Keil 或 ST-Link 将生成的固件下载到板子并运行。

**或者（CubeIDE / GCC）**
- 使用 CubeMX 打开 `.ioc` 并选择目标工具链为 GCC 或 CubeIDE，生成工程后按各自 IDE 的方法构建与下载。工程未随仓库提供完整的 Makefile，因此在非 Keil 环境下可能需要根据生成结果调整构建脚本。

**串口与日志**
- 默认串口波特率：115200，8N1。详见 `Core/Src/usart.c`（USART1/UART4 初始化）。

**关键文件与位置**
- CubeMX 配置：[h7_standard_project.ioc](h7_standard_project.ioc)
- Keil 工程：[MDK-ARM/h7_standard_project.uvprojx](MDK-ARM/h7_standard_project.uvprojx)
- 主入口：`Core/Src/main.c`
- 应用任务入口：`User/App/app_main.c`
- 串口配置：`Core/Src/usart.c`
- 中间件：`Middlewares/LVGL`, `FATFS/`, `Drivers/STM32H7xx_HAL_Driver`

**目录结构（概要）**
- `Core/` — HAL 初始化、时钟、外设初始化及中间件 glue
- `Drivers/` — CMSIS 与 STM32 HAL 驱动
- `Middlewares/` — LVGL、第三方库等
- `FATFS/`、`Target/` — FATFS 与板级支持（BSP）代码
- `User/` — 应用层代码与任务实现（`User/App/`）
- `MDK-ARM/` — Keil 工程文件与输出

**注意事项**
- 如果需要变更时钟、外设或中间件配置，请使用 STM32CubeMX 打开并修改 `h7_standard_project.ioc`，然后重新生成代码以保持代码和配置一致。
- 本仓库中包含 Keil 工程文件，若使用其他工具链请自行迁移或使用 CubeMX 生成对应工程。


