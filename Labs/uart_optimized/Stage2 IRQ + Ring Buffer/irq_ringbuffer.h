#ifndef __IRQ_RINGBUFFER_H
#define __IRQ_RINGBUFFER_H

#include "baseline.h"

// ==================== Ring Buffer Configuration ====================
#define RING_BUFFER_SIZE    16      // 缓冲区大小（必须是2的幂，方便取模优化）
#define PERIOD_MS           50      // 采样周期：50ms = 20Hz
#define PERIOD_TICKS        32250   // 50ms * 645kHz = 32250 ticks

// ==================== Ring Buffer Structure ====================
typedef struct {
    sensor_packet_t buffer[RING_BUFFER_SIZE];  // 数据缓冲区
    volatile uint32_t write_idx;               // 写指针（中断中更新）
    volatile uint32_t read_idx;                // 读指针（主循环更新）
    volatile uint32_t overflow_count;          // 溢出计数（Buffer满时丢包）
    volatile uint32_t total_samples;           // 总采样次数
} ring_buffer_t;

// ==================== Performance Statistics ====================
typedef struct {
    uint32_t isr_entry_time;        // 中断进入时间
    uint32_t isr_exit_time;         // 中断退出时间
    uint32_t max_isr_time;          // 最大中断执行时间
    uint32_t total_isr_time;        // 总中断时间（用于计算平均）
    uint32_t isr_count;             // 中断次数
    
    uint32_t main_idle_time;        // 主循环空闲时间
    uint32_t main_send_time;        // 主循环发送时间
    uint32_t last_activity_time;    // 上次活动时间
} performance_stats_t;

// ==================== Function Declarations ====================

// Ring Buffer 操作
void ring_buffer_init(void);
uint32_t ring_buffer_available(void);      // 可读数据包数量
uint32_t ring_buffer_free_space(void);     // 可写空间
int ring_buffer_write(sensor_packet_t *packet);  // 返回 0=成功, -1=满
int ring_buffer_read(sensor_packet_t *packet);   // 返回 0=成功, -1=空

// GPT1 中断配置
void gpt1_timer_init(void);
void gpt1_irq_handler(void);               // 中断服务函数

// IRQ + Ring Buffer 主循环
void irq_ringbuffer_loop(void);

// 外部访问（用于调试）
extern ring_buffer_t g_ring_buffer;
extern performance_stats_t g_perf_stats;

#endif // __IRQ_RINGBUFFER_H
