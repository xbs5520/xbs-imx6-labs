#include "irq_ringbuffer.h"
#include "../bsp/int/bsp_int.h"
#include "../bsp/led/bsp_led.h"
#include "../stdio/include/string.h" 

// ==================== Global Variables ====================
ring_buffer_t g_ring_buffer;
static uint16_t g_seq_num = 0;
static uint32_t last_send_time = 0;

uint32_t g_isr_led_count = 0;

// ==================== Ring Buffer ====================

void ring_buffer_init(void)
{
    g_ring_buffer.write_idx = 0;
    g_ring_buffer.read_idx = 0;
    g_ring_buffer.overflow_count = 0;
    g_ring_buffer.total_samples = 0;
    
    printf("[IRQ] Ring buffer initialized (size=%d)\r\n", RING_BUFFER_SIZE);
}

uint32_t ring_buffer_available(void)
{
    // 可读数据包数量
    uint32_t write = g_ring_buffer.write_idx;
    uint32_t read = g_ring_buffer.read_idx;
    
    if (write >= read) {
        return write - read;
    } else {
        return RING_BUFFER_SIZE - (read - write);
    }
}

uint32_t ring_buffer_free_space(void)
{
    // 可写空间（保留1个位置区分满/空）
    return RING_BUFFER_SIZE - ring_buffer_available() - 1;
}

int ring_buffer_write(sensor_packet_t *packet)
{
    // 检查是否有空间
    if (ring_buffer_free_space() == 0) {
        g_ring_buffer.overflow_count++;
        return -1;  // Buffer 满
    }
    
    // 写入数据 - 使用 memcpy 避免结构体赋值问题
    uint32_t write_idx = g_ring_buffer.write_idx;
    memcpy(&g_ring_buffer.buffer[write_idx], packet, sizeof(sensor_packet_t));
    
    // 更新写指针（环形）
    g_ring_buffer.write_idx = (write_idx + 1) & (RING_BUFFER_SIZE - 1);
    g_ring_buffer.total_samples++;
    
    return 0;  // 成功
}

int ring_buffer_read(sensor_packet_t *packet)
{
    // 检查是否有数据
    if (ring_buffer_available() == 0) {
        return -1;  // Buffer 空
    }
    
    // 读取数据 - 使用 memcpy 避免结构体赋值问题
    uint32_t read_idx = g_ring_buffer.read_idx;
    memcpy(packet, &g_ring_buffer.buffer[read_idx], sizeof(sensor_packet_t));
    
    // 更新读指针（环形）
    g_ring_buffer.read_idx = (read_idx + 1) & (RING_BUFFER_SIZE - 1);
    
    return 0;  // 成功
}

// ==================== GPT1 Timer Interrupt ====================

void gpt1_timer_init(void)
{
    printf("[IRQ] Initializing GPT1 timer interrupt...\r\n");
    
    // 1. 禁用 GPT1
    GPT1->CR = 0;
    
    // 2. 设置分频器：66分频 → ~645kHz
    GPT1->PR = 65;
    
    // 3. 设置初始比较值：50ms * 645kHz = 32250 ticks
    GPT1->OCR[0] = PERIOD_TICKS;
    
    // 4. 清除所有中断标志
    GPT1->SR = 0x3F;
    
    // 5. 使能输出比较中断
    GPT1->IR = 1 << 0;  // OF1IE: Output Compare 1 Interrupt Enable
    
    // 6. 配置控制寄存器 - 使用 FreeRun 模式
    // bit 9: FRR=1 (FreeRun mode - 计数器自由运行，不会自动重载)
    // bit 8-6: CLKSRC=001 (IPG clock)
    // bit 1: ENMOD=1 (使能时计数器初始化为0)
    // bit 0: EN=0 (先不使能)
    GPT1->CR = (1 << 9) | (1 << 6) | (1 << 1);
    
    // 7. 注册中断处理函数
    system_register_irqhandler(GPT1_IRQn, (system_irq_handler_t)gpt1_irq_handler, NULL);
    
    // 8. 使能 GIC 中断
    GIC_EnableIRQ(GPT1_IRQn);
    
    // 9. 启动 GPT1
    GPT1->CR |= (1 << 0);  // EN=1
    
    printf("[IRQ] GPT1 timer started: %dms period, FreeRun mode\r\n", PERIOD_MS);
}

// ==================== Interrupt Service Routine ====================

void gpt1_irq_handler(void)
{
    // 清除中断标志
    GPT1->SR = 1 << 0;
    
    // 更新下一次比较值
    GPT1->OCR[0] = GPT1->CNT + PERIOD_TICKS;
    
    // 中断计数（主循环会用来控制 LED）
    g_isr_led_count++;
    
    // 直接在中断中读取传感器并写入 Ring Buffer
    sensor_packet_t packet;
    packet.header[0] = 0xAA;
    packet.header[1] = 0x55;
    packet.seq_num = g_seq_num++;
    packet.timestamp = get_system_tick();
    
    // 读取传感器数据
    uint32_t read_start = get_system_tick();
    icm20608_read_data(&packet.accel_x, &packet.accel_y, &packet.accel_z,
                        &packet.gyro_x, &packet.gyro_y, &packet.gyro_z);
    uint32_t read_end = get_system_tick();
    
    // 性能数据
    packet.process_time_us = read_end - read_start;
    packet.send_time_us = last_send_time;  // 填充上一次的发送时间
    packet.padding = 0;
    
    // 计算 checksum
    packet.checksum = calculate_checksum(&packet);
    
    // 写入 Ring Buffer
    ring_buffer_write(&packet);
}

// ==================== Main Loop (IRQ Version) ====================

void irq_ringbuffer_loop(void)
{
    printf("\r\n");
    printf("========================================\r\n");
    printf("  Stage 2: IRQ + Ring Buffer\r\n");
    printf("========================================\r\n");
    printf("Sampling rate: %d ms (%d Hz)\r\n", PERIOD_MS, 1000/PERIOD_MS);
    printf("Buffer size: %d packets\r\n", RING_BUFFER_SIZE);
    printf("\r\n");
    
    // 显式初始化全局变量（不依赖 BSS 段清零）
    g_isr_led_count = 0;
    // g_data_ready_flag = 0;  // 不再使用
    g_seq_num = 0;
    last_send_time = 0;
    
    // 初始化
    ring_buffer_init();
    
    // 启动 GPT1 定时中断
    gpt1_timer_init();
    
    printf("[IRQ] System started. LED will blink every ~500ms.\r\n");
    printf("[IRQ] Sending data to PC...\r\n\r\n");
    
    uint32_t packets_sent = 0;
    uint32_t last_debug_time = get_system_tick();
    uint32_t last_led_check = 0;  // 上次检查 LED 的中断计数
    
    // 主循环 - 简化为单一任务：从 Buffer 读取并发送
    while(1) {
        // === 主任务：从 Ring Buffer 读取数据并发送 ===
        if (ring_buffer_available() > 0) {
            sensor_packet_t packet;
            if (ring_buffer_read(&packet) == 0) {
                // 发送并测量时间
                uint32_t send_start = get_system_tick();
                uart_send_blocking((uint8_t*)&packet, sizeof(packet));
                uint32_t send_end = get_system_tick();
                
                // 保存本次发送时间（供下一个包使用）
                last_send_time = send_end - send_start;
                packets_sent++;
            }
        }
        else {
            // === Buffer 空闲时的其他任务 ===
            // 示例：LED 闪烁控制（每 10 次中断切换一次，约 500ms）
            uint32_t current_count = g_isr_led_count;
            if ((current_count / 10) != (last_led_check / 10)) {
                led0_switch();
                last_led_check = current_count;
            }
        }

    }
}