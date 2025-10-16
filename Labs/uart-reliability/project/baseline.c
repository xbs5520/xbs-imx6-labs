#include "baseline.h"   

void baseline_loop(void)
{
    sensor_packet_t packet;
    uint16_t seq = 0;
    uint32_t last_send_time = 0;

    printf("[DEBUG] Entering baseline_loop\r\n");
    
    const uint32_t PERIOD_US = 32250;  // 50ms
    uint32_t next_tick = get_system_tick() + PERIOD_US;
    
    printf("[DEBUG] Starting main loop, will send binary data...\r\n");
    delayms(500);  // 给时间看调试信息
    
    while(1) {
        uint32_t read_start = get_system_tick();
        // blocking read
        icm20608_read_data(&packet.accel_x, &packet.accel_y, &packet.accel_z,
                           &packet.gyro_x, &packet.gyro_y, &packet.gyro_z);
        uint32_t read_end = get_system_tick();


        // write packet
        packet.header[0] = 0xAA;
        packet.header[1] = 0x55;
        packet.timestamp = get_system_tick();
        packet.process_time_us = read_end - read_start;
        packet.send_time_us = last_send_time;
        packet.seq_num = seq++;
        packet.checksum = calculate_checksum(&packet);
        

        uint32_t send_start = get_system_tick();
        // blocking send
        uart_send_blocking((uint8_t*)&packet, sizeof(packet));
        uint32_t send_end = get_system_tick();
        last_send_time = send_end - send_start;
        
        // 每100个包打印一次调试信息
        if (seq % 100 == 0) {
            printf("[DEBUG] Sent packet %d\r\n", seq);
        }
        
        while(get_system_tick() < next_tick);  // 等待到下一个周期
        next_tick += PERIOD_US;  // 计算下一个周期（消除累积误差）
    }
}

uint32_t get_system_tick() 
{
    return GPT1->CNT;
}

uint8_t calculate_checksum(sensor_packet_t *pkt) 
{
    uint8_t sum = 0;
    uint8_t *p = (uint8_t*)pkt;
    // Sum all bytes except checksum and padding (last 2 bytes)
    // Structure: header(2) + seq(2) + timestamp(4) + accel(6) + gyro(6) + process(4) + send(4) = 28 bytes
    uint32_t i = 0;
    for(; i < sizeof(sensor_packet_t) - 2; i++) {  // -2 排除 checksum 和 padding
        sum += p[i];
    }
    return sum;
}
