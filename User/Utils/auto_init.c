/**
 * @file    auto_init.c
 * @brief   自动初始化执行器 — X-Macro 表的构建、排序与执行
 * @author  Ltttttts
 *
 * ============================================================
 *  执行流程
 * ============================================================
 *
 *   auto_init_run()
 *       │
 *       ├─ 第1步：打印注册总数
 *       │
 *       ├─ 第2步：按 level 排序（插入排序）
 *       │    因为条目数通常 < 32，且大部分已经有序，
 *       │    插入排序在此场景下比快排更高效。
 *       │
 *       └─ 第3步：遍历排序后的表，逐个调用 fn()
 *            每个调用前打印 [LvlN] 函数名...
 *
 * ============================================================
 *  X-Macro 两次展开详解
 * ============================================================
 *
 *  auto_init_defs.h 中的每一行都是：
 *      AUTO_INIT_ENTRY(函数名, 优先级)
 *
 *  第一次 include：AUTO_INIT_ENTRY 定义为 extern 声明
 *      ┌─────────────────────────────────────────────┐
 *      │ #define AUTO_INIT_ENTRY(fn, lvl)            │
 *      │     extern void fn(void);                    │
 *      │ #include "auto_init_defs.h"                 │
 *      │                                             │
 *      │ // 展开后等价于：                            │
 *      │ extern void BSP_LED_Init(void);              │
 *      │ extern void Logger_Init(void);               │
 *      │ extern void BSP_LCD_Construct(void);         │
 *      │ ...                                         │
 *      └─────────────────────────────────────────────┘
 *
 *  第二次 include：AUTO_INIT_ENTRY 定义为数组元素
 *      ┌─────────────────────────────────────────────┐
 *      │ #undef AUTO_INIT_ENTRY                       │
 *      │ #define AUTO_INIT_ENTRY(fn, lvl)             │
 *      │     { (uint8_t)(lvl), (auto_init_fn_t)(fn),  │
 *      │       #fn },                                 │
 *      │                                             │
 *      │ static auto_init_entry_t s_auto_init_table[] │
 *      │     = {                                      │
 *      │ #include "auto_init_defs.h"                  │
 *      │     };                                       │
 *      │                                             │
 *      │ // 展开后等价于：                            │
 *      │ static auto_init_entry_t s_auto_init_table[] │
 *      │     = {                                      │
 *      │     { 1, BSP_LED_Init,        "BSP_LED_Init" },       │
 *      │     { 2, Logger_Init,         "Logger_Init" },        │
 *      │     { 3, BSP_LCD_Construct,   "BSP_LCD_Construct" },  │
 *      │     ...                                │
 *      │ };                                   │
 *      └─────────────────────────────────────┘
 *
 *  关键技巧：auto_init_defs.h 本身不包含任何 #define 或逻辑，
 *  它只包含 AUTO_INIT_ENTRY(...) 调用行。每次被 #include 时，
 *  根据调用者当前对 AUTO_INIT_ENTRY 的定义产生不同的展开结果。
 *  这就是 X-Macro 的"一次定义、多次展开"模式。
 */

#include "auto_init.h"       /* 类型定义、级别宏 */
#include "logger.h"          /* 日志打印 */

/* ============================================================
 *  第一遍 include：为所有初始化函数生成 extern 声明
 *
 *  此时 AUTO_INIT_ENTRY(fn, lvl) 展开为：
 *     extern void fn(void);
 *
 *  这样编译器就知道这些函数存在于其他 .c 文件中，
 *  不会因为找不到定义而报错。
 * ============================================================ */
#define AUTO_INIT_ENTRY(fn, lvl) extern void fn(void);
#include "auto_init_defs.h"

/* ============================================================
 *  第二遍 include：构建初始化函数表
 *
 *  先取消第一遍的宏定义，重新定义为数组元素格式：
 *     { (uint8_t)(lvl), (auto_init_fn_t)(fn), #fn },
 *
 *  其中：
 *    #fn  是"字符串化"运算符，把 fn 变成字符串字面量
 *         例如 fn = BSP_LED_Init → #fn = "BSP_LED_Init"
 *    (auto_init_fn_t)(fn) 是强制类型转换，把具体函数指针
 *         转为统一的 void (*)(void) 类型
 * ============================================================ */
#undef AUTO_INIT_ENTRY
#define AUTO_INIT_ENTRY(fn, lvl) \
    { (uint8_t)(lvl), (auto_init_fn_t)(fn), #fn },

static auto_init_entry_t s_auto_init_table[] = {
#include "auto_init_defs.h"
};

/**
 * 计算表项数量。
 * sizeof(table) / sizeof(table[0]) 是 C 语言中
 * 获取静态数组元素个数的标准写法，编译期计算，零运行时开销。
 */
static const uint32_t s_auto_init_count =
    sizeof(s_auto_init_table) / sizeof(s_auto_init_table[0]);

/* ============================================================
 *  插入排序（按 level 升序）
 *
 *  为什么用插入排序而不是 qsort？
 *    1. 条目数通常 < 32，插入排序对此规模很高效
 *    2. auto_init_defs.h 中条目已经按 level 分组书写，
 *       大部分情况下已接近有序，插入排序接近 O(n)
 *    3. 不引入 <stdlib.h> 依赖，更轻量
 *
 *  排序原理：
 *    类似打牌时整理手牌，每取一张新牌（s_auto_init_table[i]），
 *    在已排序的部分（[0..i-1]）中找到它该放的位置，插入进去。
 * ============================================================ */
static void prv_sort_by_level(void)
{
    /* 0 个或 1 个条目不需要排序 */
    if (s_auto_init_count <= 1) {
        return;
    }

    /* 从第 2 个元素开始，逐个插入到已排序序列中 */
    for (uint32_t i = 1; i < s_auto_init_count; i++) {
        /* 取出当前要插入的元素 */
        auto_init_entry_t key = s_auto_init_table[i];
        int32_t j = (int32_t)i - 1;

        /* 从后往前扫描已排序的部分，找到插入位置 */
        while (j >= 0 &&
               s_auto_init_table[j].level > key.level) {
            /* 比 key 大的元素往后移一位 */
            s_auto_init_table[j + 1] = s_auto_init_table[j];
            j--;
        }
        /* 把 key 放到正确的位置 */
        s_auto_init_table[j + 1] = key;
    }
}

/* ============================================================
 *  执行所有初始化函数
 *
 *  调用者应把此函数放在 StartTask 的临界区内：
 *    taskENTER_CRITICAL();
 *    auto_init_run();
 *    taskEXIT_CRITICAL();
 *
 *  在临界区内执行的原因是：初始化期间 RTOS 调度器尚未就绪，
 *  需要防止其他任务在初始化中途被调度运行。
 * ============================================================ */
void auto_init_run(void)
{
    /* 打印一共有多少个模块注册了初始化 */
    LOG_I("INIT", "Auto-init: %u entries registered",
          s_auto_init_count);

    /* 先排序，再执行（确保依赖顺序正确） */
    prv_sort_by_level();

    /* 遍历排序后的表，逐个调用初始化函数 */
    for (uint32_t i = 0; i < s_auto_init_count; i++) {
        /* 用 const 指针读取，不修改表内容 */
        const auto_init_entry_t *e = &s_auto_init_table[i];

        /* 防御性检查：函数指针不应为 NULL */
        if (e->fn != NULL) {
            /* 打印当前正在执行哪个模块 */
            LOG_I("INIT", "[Lvl%u] %s...",
                  e->level, e->name);

            /* 执行初始化函数 */
            e->fn();
        }
    }

    /* 全部初始化完成 */
    LOG_I("INIT", "Auto-init complete.");
}
