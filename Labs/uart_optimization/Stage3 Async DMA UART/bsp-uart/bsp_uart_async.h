#ifndef _BSP_UART_ASYNC_H
#define _BSP_UART_ASYNC_H

#include "../../imx6ul/MCIMX6Y2.h"
#include "../../stdio/include/types.h"
#define false 0
#define true 1
#define bool int
// ==================== UART 异步发送模块 ====================
// Stage 3: 使用 UART TX 中断实现非阻塞异步发送
// 目标: CPU 不阻塞在 UART 发送上，可以处理其他任务
// 原理: 利用 UART TX FIFO 空中断，每次中断发送一个字节

// ==================== Configuration ====================

// TX 缓冲区大小（必须 >= sizeof(sensor_packet_t) = 30）
#define UART_ASYNC_TX_BUFFER_SIZE   64

// ==================== Data Structures ====================

// 异步发送统计信息
typedef struct {
    uint32_t total_bytes;       // 总发送字节数
    uint32_t total_packets;     // 总发送包数
    uint32_t total_interrupts;  // 总中断次数
    uint32_t errors;            // 错误次数（发送时 busy）
} uart_async_stats_t;

// ==================== Function Prototypes ====================

/**
 * @brief 初始化 UART 异步发送模块
 * 
 * 配置 UART TX 中断，注册中断处理函数
 * 必须在 uart_init() 之后调用
 */
void uart_async_init(void);

/**
 * @brief 启动异步发送
 * 
 * @param data 要发送的数据指针
 * @param len  数据长度（字节）
 * @return int 0=成功启动，-1=忙（上次发送未完成），-2=参数错误
 * 
 * 注意：
 * - 函数会立即返回（不阻塞）
 * - 数据会被复制到内部缓冲区
 * - 实际发送在中断中完成
 */
int uart_async_send(uint8_t *data, uint32_t len);

/**
 * @brief 检查发送是否忙
 * 
 * @return bool true=正在发送，false=空闲
 */
bool uart_async_is_busy(void);

/**
 * @brief 等待发送完成
 * 
 * 阻塞等待当前发送完成
 * 用于需要确保数据发送完成的场景（如关机前）
 */
void uart_async_wait_complete(void);

/**
 * @brief 获取统计信息
 * 
 * @return uart_async_stats_t* 统计信息指针
 */
uart_async_stats_t* uart_async_get_stats(void);

/**
 * @brief UART TX 中断处理函数
 * 
 * 内部函数，由中断系统调用
 * 不要直接调用！
 */
void uart1_tx_irq_handler(void);

#endif // _BSP_UART_ASYNC_H
