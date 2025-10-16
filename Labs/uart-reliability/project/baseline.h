#ifndef __BASELINE_H
#define __BASELINE_H

#include <unistd.h> 
#include "../imx6ul/imx6ul.h"
#include "../bsp/led/bsp_led.h"
#include "../bsp/delay/bsp_delay.h"
#include "../bsp/clk/bsp_clk.h"
#include "../bsp/beep/bsp_beep.h"
#include "../bsp/key/bsp_key.h"
#include "../bsp/int/bsp_int.h"
#include "../bsp/exit/bsp_exit.h"
#include "../bsp/epittimer/bsp_epittimer.h"
#include "../bsp/key_filter/bsp_keyfilter.h"
#include "../bsp/uart/bsp_uart.h"
#include "../stdio/include/stdio.h"
#include "../bsp/lcd/bsp_lcd.h"
#include "../bsp/lcd/bsp_lcdapi.h"
#include "../bsp/rtc/bsp_rtc.h"
#include "../bsp/ap3216c/bsp_ap3216c.h"
#include "../bsp/icm20608/bsp_icm20608.h"

typedef struct {
    uint8_t header[2];         // head 0xAA 0x55
    uint16_t seq_num;          // seq (移到前面，2+2=4字节对齐)
    uint32_t timestamp;        // timestamp （GPT1 ticks，~645kHz）
    int16_t accel_x;           // 加速度X（原始ADC值）
    int16_t accel_y;           // 加速度Y
    int16_t accel_z;           // 加速度Z
    int16_t gyro_x;            // 陀螺仪X（原始ADC值）
    int16_t gyro_y;            // 陀螺仪Y
    int16_t gyro_z;            // 陀螺仪Z
    uint32_t process_time_us;  // read（GPT1 ticks）
    uint32_t send_time_us;     // sendtime（GPT1 ticks）
    uint8_t checksum;          // check by sum
    uint8_t padding;           // 填充到偶数字节 (29->30字节)
} __attribute__((packed)) sensor_packet_t;


void baseline_loop(void);
void baseline_loop_simple_test(void);  // 简单测试版本
void uart_raw_test(void);              // 文本测试
void uart_binary_test(void);           // 二进制测试
uint8_t calculate_checksum(sensor_packet_t *pkt);
uint32_t get_system_tick();

#endif //__BASELINE_H