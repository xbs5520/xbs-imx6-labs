#include "bsp_uart_async.h"
#include "bsp_uart.h"
#include "../int/bsp_int.h"
#include "../../stdio/include/string.h"
#include "../../stdio/include/stdio.h"

// ==================== Private Variables ====================

// TX 缓冲区（存放待发送的数据）
static uint8_t uart_tx_buffer[UART_ASYNC_TX_BUFFER_SIZE];

// 发送状态
static uint32_t uart_tx_len;           // 本次要发送的字节数
static uint32_t uart_tx_idx;           // 当前发送到第几个字节
static volatile bool uart_tx_busy;     // 是否正在发送

// 性能统计
static uart_async_stats_t g_stats;

// ==================== Public Functions ====================

void uart_async_init(void)
{
    // 1. 初始化全局变量
    uart_tx_len = 0;
    uart_tx_idx = 0;
    uart_tx_busy = false;
    
    // 2. 初始化统计信息
    memset(&g_stats, 0, sizeof(g_stats));
    
    // 3. 配置 TX FIFO 触发阈值
    // UFCR bits 10-15: TXTL (TX Trigger Level)
    // 设置为 2：当 FIFO 中少于 2 个字节时触发中断
    uint32_t ufcr = UART1->UFCR;
    ufcr &= ~(0x3F << 10);  // 清除 TXTL bits
    ufcr |= (2 << 10);       // 设置 TXTL = 2
    UART1->UFCR = ufcr;
    
    // 4. 确保 TX 中断初始状态为禁用
    // UCR1 bit 13: TRDYEN (Transmitter Ready Interrupt Enable)
    UART1->UCR1 &= ~(1 << 13);
    
    // 5. 注册 UART1 中断处理函数
    system_register_irqhandler(UART1_IRQn, (system_irq_handler_t)uart1_tx_irq_handler, NULL);
    
    // 6. 使能 GIC 中断
    GIC_EnableIRQ(UART1_IRQn);
    
    printf("[ASYNC] UART async TX initialized\r\n");
    printf("[ASYNC] Buffer size: %d bytes\r\n", UART_ASYNC_TX_BUFFER_SIZE);
    printf("[ASYNC] 11UFCR=0x%08X (TXTL=%u)\r\n", UART1->UFCR, (UART1->UFCR >> 10) & 0x3F);
}

int uart_async_send(uint8_t *data, uint32_t len)
{
    // === 参数检查 ===
    if (data == NULL || len == 0) {
        return -2;  // 参数错误
    }
    
    if (len > UART_ASYNC_TX_BUFFER_SIZE) {
        return -2;  // 数据太长
    }
    
    // === 检查是否忙 ===
    if (uart_tx_busy) {
        g_stats.errors++;
        return -1;  // 上一次发送还没完成
    }
    
    // === 复制数据到 TX 缓冲区 ===
    // 为什么要复制？因为调用者的 data 可能会被修改
    // 例如：ring buffer 的下一次 read 会覆盖同一个位置
    memcpy(uart_tx_buffer, data, len);
    
    // === 初始化发送状态 ===
    uart_tx_len = len;
    uart_tx_idx = 0;
    uart_tx_busy = true;
    
    // === 更新统计 ===
    g_stats.total_bytes += len;
    g_stats.total_packets++;
    
    // === 启动发送：使能 UART TX 中断 ===
    UART1->UCR1 |= (1 << 13);
    
    // === 立即返回！CPU 不用等待 ===
    return 0;
}

bool uart_async_is_busy(void)
{
    return uart_tx_busy;
}

void uart_async_wait_complete(void)
{
    // 阻塞等待发送完成
    while (uart_tx_busy) {
        // 空等待
        // 也可以添加超时保护
    }
}

uart_async_stats_t* uart_async_get_stats(void)
{
    return &g_stats;
}

// ==================== Interrupt Handler ====================

void uart1_tx_irq_handler(void)
{
    // === 读取 UART 状态寄存器 ===
    uint32_t status1 = UART1->USR1;
    
    // === 检查 TX Ready 标志 ===
    // USR1 bit 13: TRDY (Transmitter Ready)
    if (status1 & (1 << 13)) {
        // === 发送一个字节 ===
        if (uart_tx_idx < uart_tx_len) {
            UART1->UTXD = uart_tx_buffer[uart_tx_idx] & 0xFF;
            uart_tx_idx++;
            g_stats.total_interrupts++;
        }
        
        // === 检查是否全部发送完成 ===
        if (uart_tx_idx >= uart_tx_len) {
            // 禁用 TX 中断
            UART1->UCR1 &= ~(1 << 13);
            // 标记为空闲
            uart_tx_busy = false;
        }
    }
}
