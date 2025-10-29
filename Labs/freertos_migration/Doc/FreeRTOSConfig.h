#ifndef __FREERTOSCONFIG_H
#define __FREERTOSCONFIG_H
#include "../imx6ul/imx6ul.h"
// basic config
#define configUSE_PREEMPTION                        1           //preemt
#define configCPU_CLOCK_HZ                          528000000   //528Mhz
#define configTICK_RATE_HZ                          1000        // 1ms
#define configMAX_PRIORITIES                        5           // 5
#define configMINIMAL_STACK_SIZE                    128         //128
#define configTOTAL_HEAP_SIZE                       (24 * 1024)  // 12KB (stats函数需要额外内存)
#define configMAX_TASK_NAME_LEN                     16
#define configUSE_16_BIT_TICKS                      0           // Tick 计数器位数（ARM 用 0 = 32位）

// function         
#define configUSE_MUTEXES                           1
#define configUSE_RECURSIVE_MUTEXES                 0
#define configUSE_COUNTING_SEMAPHORES               1
#define configUSE_TIMERS                            1
#define configUSE_IDLE_HOOK                         0
#define configUSE_TICK_HOOK                         0
#define configUSE_MALLOC_FAILED_HOOK                1
#define configCHECK_FOR_STACK_OVERFLOW              2

// software timer
#define configTIMER_TASK_PRIORITY                   1
#define configTIMER_QUEUE_LENGTH                    10
#define configTIMER_TASK_STACK_DEPTH                256

// API
#define INCLUDE_vTaskDelay                          1
#define INCLUDE_vTaskDelayUntil                     1
#define INCLUDE_vTaskDelete                         1
#define INCLUDE_xTaskGetSchedulerState              1

// assert with debug info
extern int printf(const char *fmt, ...);
#define configASSERT( x ) if( ( x ) == 0 ) { \
    printf("\r\n[ASSERT FAILED] %s:%d\r\n", __FILE__, __LINE__); \
    taskDISABLE_INTERRUPTS(); \
    for( ;; ); \
}

/* ===== ARM Cortex-A 特定配置 ===== */
/* GIC 中断控制器地址（IMX6ULL 特定）
 * 注意：FreeRTOS 期望这里是 GIC Distributor 基址，而不是 GIC 总基址！
 * IMX6ULL GIC 寄存器分布：
 *   - GIC 总基址: 0x00A00000
 *   - Distributor: 0x00A00000 + 0x1000 = 0x00A01000
 *   - CPU Interface: 0x00A00000 + 0x2000 = 0x00A02000
 */
#define configINTERRUPT_CONTROLLER_BASE_ADDRESS             0x00A01000  /* GIC Distributor 基址 */
#define configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET     0x1000       /* 从 Distributor 到 CPU Interface 的偏移 (0x00A02000 - 0x00A01000) */
#define configINTERRUPT_CONTROLLER_DISTRIBUTOR_OFFSET       0x0000       /* 已经指向 Distributor，偏移为 0 */

/* 中断优先级配置 - 必须匹配 GIC 硬件 */
/* IMX6ULL GIC-400 支持 32 个优先级 (0-31)，数字越小优先级越高 */
#define configUNIQUE_INTERRUPT_PRIORITIES           32      /* GIC 支持 32 个优先级 */
#define configMAX_API_CALL_INTERRUPT_PRIORITY       20      /* 优先级 20，满足 > 32/2 且 <= 32 */

/* 定时器配置（使用你已有的 GPT1 或创建新定时器）*/
extern void vConfigureTickInterrupt(void);
extern void vClearTickInterrupt(void);
#define configSETUP_TICK_INTERRUPT()    vConfigureTickInterrupt()
#define configCLEAR_TICK_INTERRUPT()    vClearTickInterrupt()

#define configGENERATE_RUN_TIME_STATS               1
#define configUSE_TRACE_FACILITY                    1
#define configUSE_STATS_FORMATTING_FUNCTIONS        1

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() /* 空，GPT2 已初始化 */
#define portGET_RUN_TIME_COUNTER_VALUE()         GPT2->CNT

#endif //__FREERTOSCONFIG_H