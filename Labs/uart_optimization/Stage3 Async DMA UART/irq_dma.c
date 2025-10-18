#include "irq_dma.h"
#include "../bsp/int/bsp_int.h"
#include "../bsp/led/bsp_led.h"
#include "../bsp/uart/bsp_uart_async.h"  // ← 使用异步 UART
#include "../stdio/include/string.h"
#include "../stdio/include/stdio.h"

// ==================== Global Variables ====================

ring_buffer_t g_ring_buffer_dma;  // 重命名避免冲突
static uint16_t g_seq_num_dma = 0;
static uint32_t last_send_time_dma = 0;

// ISR 用的全局变量
uint32_t g_isr_led_count_dma = 0;

// ==================== Ring Buffer Implementation ====================
// 与 Stage 2 完全相同，但函数名加 _dma 后缀

void ring_buffer_dma_init(void)
{
    g_ring_buffer_dma.write_idx = 0;
    g_ring_buffer_dma.read_idx = 0;
    g_ring_buffer_dma.overflow_count = 0;
    g_ring_buffer_dma.total_samples = 0;
    
    printf("[DMA] Ring buffer initialized (size=%d)\r\n", RING_BUFFER_SIZE);
}

uint32_t ring_buffer_dma_available(void)
{
    uint32_t write = g_ring_buffer_dma.write_idx;
    uint32_t read = g_ring_buffer_dma.read_idx;
    
    if (write >= read) {
        return write - read;
    } else {
        return RING_BUFFER_SIZE - (read - write);
    }
}

uint32_t ring_buffer_dma_free_space(void)
{
    return RING_BUFFER_SIZE - ring_buffer_dma_available() - 1;
}

int ring_buffer_dma_write(sensor_packet_t *packet)
{
    if (ring_buffer_dma_free_space() == 0) {
        g_ring_buffer_dma.overflow_count++;
        return -1;
    }
    
    uint32_t write_idx = g_ring_buffer_dma.write_idx;
    memcpy(&g_ring_buffer_dma.buffer[write_idx], packet, sizeof(sensor_packet_t));
    
    g_ring_buffer_dma.write_idx = (write_idx + 1) & (RING_BUFFER_SIZE - 1);
    g_ring_buffer_dma.total_samples++;
    
    return 0;
}

int ring_buffer_dma_read(sensor_packet_t *packet)
{
    if (ring_buffer_dma_available() == 0) {
        return -1;
    }
    
    uint32_t read_idx = g_ring_buffer_dma.read_idx;
    memcpy(packet, &g_ring_buffer_dma.buffer[read_idx], sizeof(sensor_packet_t));
    
    g_ring_buffer_dma.read_idx = (read_idx + 1) & (RING_BUFFER_SIZE - 1);
    
    return 0;
}

// ==================== GPT1 Timer Interrupt ====================
// 与 Stage 2 完全相同，但函数名加 _dma 后缀

void gpt1_timer_dma_init(void)
{
    printf("[DMA] Initializing GPT1 timer interrupt...\r\n");
    
    GPT1->CR = 0;
    GPT1->PR = 65;
    GPT1->OCR[0] = PERIOD_TICKS;
    GPT1->SR = 0x3F;
    GPT1->IR = 1 << 0;
    GPT1->CR = (1 << 9) | (1 << 6) | (1 << 1);
    
    system_register_irqhandler(GPT1_IRQn, (system_irq_handler_t)gpt1_irq_handler_dma, NULL);
    GIC_EnableIRQ(GPT1_IRQn);
    
    GPT1->CR |= (1 << 0);
    
    printf("[DMA] GPT1 timer started: %dms period, FreeRun mode\r\n", PERIOD_MS);
}

void gpt1_irq_handler_dma(void)
{
    GPT1->SR = 1 << 0;
    GPT1->OCR[0] = GPT1->CNT + PERIOD_TICKS;
    
    g_isr_led_count_dma++;
    
    // 读取传感器并写入 Ring Buffer
    sensor_packet_t packet;
    packet.header[0] = 0xAA;
    packet.header[1] = 0x55;
    packet.seq_num = g_seq_num_dma++;
    packet.timestamp = get_system_tick();
    
    uint32_t read_start = get_system_tick();
    icm20608_read_data(&packet.accel_x, &packet.accel_y, &packet.accel_z,
                        &packet.gyro_x, &packet.gyro_y, &packet.gyro_z);
    uint32_t read_end = get_system_tick();
    
    packet.process_time_us = read_end - read_start;
    packet.send_time_us = last_send_time_dma;
    packet.padding = 0;
    packet.checksum = calculate_checksum(&packet);
    
    ring_buffer_dma_write(&packet);
}

// ==================== Main Loop (Stage 3: Async UART) ====================

void irq_dma_loop(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  Stage 3: IRQ + Ring Buffer + Async TX\r\n");
    printf("========================================\r\n");
    printf("Sampling rate: %d ms (%d Hz)\r\n", PERIOD_MS, 1000/PERIOD_MS);
    printf("Buffer size: %d packets\r\n", RING_BUFFER_SIZE);
    printf("TX Mode: Asynchronous (Interrupt-driven)\r\n");
    printf("\r\n");
    
    // 显式初始化全局变量
    g_isr_led_count_dma = 0;
    g_seq_num_dma = 0;
    last_send_time_dma = 0;
    
    // 初始化各模块
    ring_buffer_dma_init();
    uart_async_init();    // ← 初始化异步 UART
    gpt1_timer_dma_init();
    
    printf("[DMA] System started. LED will blink every ~500ms.\r\n");
    printf("[DMA] Sending data to PC (async mode)...\r\n\r\n");
    
    uint32_t packets_sent = 0;
    uint32_t last_led_check = 0;
    uint32_t last_stats_time = get_system_tick();
    
    // 主循环
    while(1) {
        // ===== 任务 1：异步发送数据 =====
        // 关键改变：uart_async_send() 立即返回，不阻塞！
        if (ring_buffer_dma_available() > 0 && !uart_async_is_busy()) {
            sensor_packet_t packet;
            if (ring_buffer_dma_read(&packet) == 0) {
                // 测量启动时间（应该非常短，~1μs）
                uint32_t send_start = get_system_tick();
                
                // 启动异步发送（立即返回！）
                int ret = uart_async_send((uint8_t*)&packet, sizeof(packet));
                
                uint32_t send_end = get_system_tick();
                
                if (ret == 0) {
                    // 成功启动
                    last_send_time_dma = send_end - send_start;  // 应该接近 0
                    packets_sent++;
                } else {
                    // 发送失败（应该不会发生，因为我们检查了 busy）
                    printf("[DMA] Warning: async send failed, ret=%d\r\n", ret);
                }
                
                // ← CPU 立即可以继续，不用等待 4ms！
            }
        }
        
        // ===== 任务 2：LED 控制 =====
        // 现在 CPU 有更多空闲时间来处理这个任务
        uint32_t current_count = g_isr_led_count_dma;
        if ((current_count / 10) != (last_led_check / 10)) {
            led0_switch();
            last_led_check = current_count;
        }
        
        // ===== 任务 3：定期打印统计信息 =====
        // 每 5 秒打印一次（可选，用于调试）
        uint32_t current_time = get_system_tick();
        if (current_time - last_stats_time > 3225000) {  // 5 秒
            uart_async_stats_t *stats = uart_async_get_stats();
            printf("[DMA] Stats: packets=%u, bytes=%u, interrupts=%u, errors=%u\r\n",
                   stats->total_packets, stats->total_bytes, 
                   stats->total_interrupts, stats->errors);
            printf("[DMA] Ring: available=%u, overflow=%u\r\n",
                   ring_buffer_dma_available(), g_ring_buffer_dma.overflow_count);
            last_stats_time = current_time;
        }
        
    }
}
