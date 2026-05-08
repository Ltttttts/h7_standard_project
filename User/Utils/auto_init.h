/**
 * @file    auto_init.h
 * @brief   自动初始化机制 — 公共接口和类型定义
 * @author  Ltttttts
 *
 * ============================================================
 *  设计思想（参考 Linux 内核的 initcall 机制）
 * ============================================================
 *
 * 传统做法：在 StartTask 中逐行手动调用各个模块的初始化函数。
 *   弊端：每加一个新模块就要改 StartTask，耦合高、易遗漏。
 *
 * 本方案：每个模块在 auto_init_defs.h 中"登记"一行，
 *        系统启动时 auto_init_run() 自动按优先级顺序执行。
 *   好处：模块完全解耦，新增模块只需加一行，无需修改任何已有代码。
 *
 * ============================================================
 *  核心实现 — X-Macro 技巧
 * ============================================================
 *
 * X-Macro 是 C 语言的一种预处理器元编程技巧：
 *   1. 在 auto_init_defs.h 中，所有条目都用同一个宏 AUTO_INIT_ENTRY() 表达
 *   2. 在 auto_init.c 中，两次 #include 这个文件：
 *      - 第一次：把 AUTO_INIT_ENTRY 展开为 extern 声明（预声明函数）
 *      - 第二次：把 AUTO_INIT_ENTRY 展开为结构体数组元素（构建数据表）
 *   3. 同一个文件被 "复用" 两次，产生不同代码，零重复、零运行时开销
 *
 * ============================================================
 *  初始化级别（值越小越先执行）
 * ============================================================
 *
 * Level 1 (BOARD)     板级硬件初始化        如引脚配置、时钟使能
 * Level 2 (PREV)      纯软件基础设施         如日志系统、内存管理
 * Level 3 (DEVICE)    设备驱动注册           如 LCD、UART 设备对象
 * Level 4 (COMPONENT) 组件配置              如 ADC 通道、传感器
 * Level 5 (ENV)       环境初始化             如文件系统挂载
 * Level 6 (APP)       应用层启动             如 RTOS 任务创建
 * Level 7 (READY)     系统就绪标记           如开机自检完成通知
 */

#ifndef AUTO_INIT_H
#define AUTO_INIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 初始化级别宏 ========== */

/** 板级硬件初始化：引脚复用、时钟配置等 */
#define AUTO_INIT_LEVEL_BOARD       (1U)

/** 纯软件基础设施：日志系统、堆管理器等 */
#define AUTO_INIT_LEVEL_PREV        (2U)

/** 设备驱动注册：创建 UART/SPI/I2C 设备对象 */
#define AUTO_INIT_LEVEL_DEVICE      (3U)

/** 组件配置：传感器参数、ADC 通道等 */
#define AUTO_INIT_LEVEL_COMPONENT   (4U)

/** 环境初始化：文件系统挂载等 */
#define AUTO_INIT_LEVEL_ENV         (5U)

/** 应用层启动：创建 RTOS 业务任务 */
#define AUTO_INIT_LEVEL_APP         (6U)

/** 系统就绪标记：所有初始化完成 */
#define AUTO_INIT_LEVEL_READY       (7U)

/* ========== 类型定义 ========== */

/**
 * @brief 初始化函数类型。
 *        所有通过 AUTO_INIT_ENTRY 注册的函数必须满足此签名。
 *        返回值：void
 *        参数：无
 */
typedef void (*auto_init_fn_t)(void);

/**
 * @brief 初始化表项结构体。
 *        每个模块对应一个表项，记录了该模块的初始化优先级、
 *        要调用的函数指针和用于日志打印的名字。
 *
 *        level ─ 初始化优先级（1~7，值越小越先执行）
 *        fn    ─ 要调用的初始化函数指针
 *        name  ─ 函数名字符串（用于日志输出，方便调试）
 */
typedef struct auto_init_entry {
    uint8_t        level;   /* 优先级：AUTO_INIT_LEVEL_xxx */
    auto_init_fn_t fn;      /* 函数指针：void (*)(void)   */
    const char    *name;    /* 函数名：如 "BSP_LED_Init"  */
} auto_init_entry_t;

/* ========== 公开接口 ========== */

/**
 * @brief 按优先级顺序执行所有注册的初始化函数。
 *        在 StartTask 的临界区内调用一次即可。
 *
 * 使用示例：
 *   void StartTask_Entry(void *argument) {
 *       taskENTER_CRITICAL();
 *       auto_init_run();   // ← 所有注册的初始化自动按序执行
 *       taskEXIT_CRITICAL();
 *       osThreadExit();
 *   }
 */
void auto_init_run(void);

#ifdef __cplusplus
}
#endif

#endif /* AUTO_INIT_H */
