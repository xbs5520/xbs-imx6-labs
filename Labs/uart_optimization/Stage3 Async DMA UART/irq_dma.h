#ifndef _IRQ_DMA_H
#define _IRQ_DMA_H

#include "../imx6ul/MCIMX6Y2.h"
#include "../stdio/include/types.h"
#include "baseline.h"  // 使用相同的 sensor_packet_t 定义
#include "irq_ringbuffer.h"
// ==================== Stage 3: IRQ + Ring Buffer + Async UART ====================
// 在 Stage 2 基础上，将 UART 发送从阻塞改为异步（中断驱动）
// 目标：进一步降低 CPU 占用率，提升系统响应性

// ==================== Configuration ====================

// Ring Buffer 配置（与 Stage 2 相同）
#define RING_BUFFER_SIZE    16      // 16 个数据包（2的幂次）
#define PERIOD_MS           50      // 采样周期：50ms = 20Hz
#define PERIOD_TICKS        32250   // 50ms * 645kHz = 32250 ticks

extern ring_buffer_t g_ring_buffer;
extern uint32_t g_isr_led_count;

// ==================== Function Declarations ====================

// Ring Buffer 管理（DMA 版本，加后缀避免冲突）
void ring_buffer_dma_init(void);
uint32_t ring_buffer_dma_available(void);
uint32_t ring_buffer_dma_free_space(void);
int ring_buffer_dma_write(sensor_packet_t *packet);
int ring_buffer_dma_read(sensor_packet_t *packet);

// GPT1 定时器（DMA 版本，加后缀避免冲突）
void gpt1_timer_dma_init(void);
void gpt1_irq_handler_dma(void);

// Stage 3 主循环
void irq_dma_loop(void);

#endif // _IRQ_DMA_H
