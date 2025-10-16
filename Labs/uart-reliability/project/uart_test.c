#include "baseline.h"

// 最简单的测试 - 只用 printf 发送可见字符
void uart_raw_test(void)
{
    printf("[RAW TEST] Starting raw UART test...\r\n");
    delayms(1000);
    
    uint32_t count = 0;
    
    while(1) {
        printf("[RAW] Packet %d: ", count);
        
        // 发送帧头（可见字符形式）
        printf("AA 55 ");
        
        // 发送一些数据
        int i = 0;
        for(; i < 10; i++) {
            printf("%02X ", count + i);
        }
        
        printf("\r\n");
        
        count++;
        delayms(1000);  // 1秒一个
    }
}

// 纯二进制测试 - 用 putc 发送
void uart_binary_test(void)
{
    printf("[BINARY TEST] Starting binary test...\r\n");
    printf("Will send: AA 55 00 01 02 03 ... repeatedly\r\n");
    delayms(2000);
    
    uint8_t counter = 0;
    
    while(1) {
        // 发送帧头
        putc(0xAA);
        putc(0x55);
        
        // 发送10个递增的字节
        uint8_t i = 0;
        for(; i < 10; i++) {
            putc(counter + i);
        }
        
        counter++;
        
        // 每10次打印一次状态
        if(counter % 10 == 0) {
            printf("[BINARY] Sent %d packets\r\n", counter);
        }
        
        delayms(100);  // 100ms 一个包
    }
}
