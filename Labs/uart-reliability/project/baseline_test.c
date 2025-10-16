// Baseline Test & Verification
// 用于验证 baseline 代码的正确性

#include "baseline.h"

void baseline_verify(void)
{
    // 1. 验证数据包大小
    printf("=== Baseline Verification ===\r\n");
    printf("Packet size: %d bytes\r\n", sizeof(sensor_packet_t));
    printf("Expected: 29 bytes\r\n");
    printf("  header:       2 bytes\r\n");
    printf("  timestamp:    4 bytes\r\n");
    printf("  seq_num:      2 bytes\r\n");
    printf("  accel (x3):   6 bytes\r\n");
    printf("  gyro (x3):    6 bytes\r\n");
    printf("  process_time: 4 bytes\r\n");
    printf("  send_time:    4 bytes\r\n");
    printf("  checksum:     1 byte\r\n");
    printf("\r\n");
    
    // 2. 测试传感器读取
    int16_t ax, ay, az, gx, gy, gz;
    
    // 预热：第一次读取可能较慢
    icm20608_read_data(&ax, &ay, &az, &gx, &gy, &gz);
    
    // 精确测试：连续读取10次取平均
    uint32_t total_time = 0;
    int i = 0;
    for(; i < 10; i++) {
        uint32_t start = get_system_tick();
        icm20608_read_data(&ax, &ay, &az, &gx, &gy, &gz);
        uint32_t elapsed = get_system_tick() - start;
        total_time += elapsed;
    }
    uint32_t avg_time = total_time / 10;
    
    printf("Sensor read test:\r\n");
    printf("  Accel: X=%d, Y=%d, Z=%d\r\n", ax, ay, az);
    printf("  Gyro:  X=%d, Y=%d, Z=%d\r\n", gx, gy, gz);
    printf("  Average time (10 reads): %d us\r\n", avg_time);
    printf("  Expected: ~20 us @ 6MHz SPI\r\n");
    printf("\r\n");
    
    // 3. 测试UART发送时间
    sensor_packet_t test_packet = {0};
    test_packet.header[0] = 0xAA;
    test_packet.header[1] = 0x55;
    
    uint32_t start = get_system_tick();
    uart_send_blocking((uint8_t*)&test_packet, sizeof(test_packet));
    uint32_t elapsed = get_system_tick() - start;
    
    printf("UART send test:\r\n");
    printf("  Packet size: %d bytes\r\n", sizeof(test_packet));
    printf("  Time: %d us\r\n", elapsed);
    // 修正计算公式
    printf("  Throughput: %d bytes/s (expected ~11520 @ 115200 bps)\r\n", 
           (int)((sizeof(test_packet) * 1000000UL) / elapsed));
    printf("\r\n");
    
    // 4. 测试GPT1定时器
    uint32_t tick1 = get_system_tick();
    delayms(10);
    uint32_t tick2 = get_system_tick();
    
    printf("Timer accuracy test:\r\n");
    printf("  Expected delay: 10000 us\r\n");
    printf("  Actual delay:   %d us\r\n", tick2 - tick1);
    printf("  Error: %d us (%.2f%%)\r\n", 
           (int)(tick2 - tick1 - 10000),
           ((float)(tick2 - tick1 - 10000) / 10000.0) * 100.0);
    printf("\r\n");
    
    printf("=== Verification Complete ===\r\n\r\n");
}
