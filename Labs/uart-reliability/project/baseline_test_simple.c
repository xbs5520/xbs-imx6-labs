#include "baseline.h"
#include "../stdio/include/string.h"

// 简化版测试函数 - 只发送数据包，不读传感器
void baseline_loop_simple_test(void)
{
    printf("[TEST] Simple packet test starting...\r\n");
    delayms(1000);
    
    printf("[TEST] Creating packet structure...\r\n");
    sensor_packet_t packet;
    uint16_t seq = 0;
    
    printf("[TEST] Entering main loop...\r\n");
    delayms(500);
    
    while(1) {
        printf("[TEST] Loop iteration %d\r\n", seq);
        
        // 填充数据包
        packet.header[0] = 0xAA;
        packet.header[1] = 0x55;
        packet.seq_num = seq++;
        packet.timestamp = get_system_tick();  // 现在可以安全使用了！
        
        // 测试数据
        packet.accel_x = 100;
        packet.accel_y = 200;
        packet.accel_z = 300;
        packet.gyro_x = 10;
        packet.gyro_y = 20;
        packet.gyro_z = 30;
        
        packet.process_time_us = 1000;
        packet.send_time_us = 2000;
        packet.padding = 0;  // 填充字节清零
        
        packet.checksum = calculate_checksum(&packet);
        
        printf("[TEST] Sending %d bytes...\r\n", sizeof(packet));
        
        // 方式1：使用 putc 逐字节发送（这个肯定能工作，因为printf用的就是它）
        uint8_t *pdata = (uint8_t*)&packet;
        uint32_t i = 0;
        for(; i < sizeof(packet); i++) {
            putc(pdata[i]);
        }
        
        // 方式2：如果上面能工作，再试 uart_send_blocking
        // uart_send_blocking((uint8_t*)&packet, sizeof(packet));
        
        printf("[TEST] Sent packet %d successfully!\r\n", seq);
        
        // 等待 1秒（方便看调试信息）
        delayms(1000);
    }
}
