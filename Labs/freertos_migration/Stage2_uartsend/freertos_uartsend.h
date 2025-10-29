#ifndef __FREERTOS_UARTSEND_H
#define __FREERTOS_UARTSEND_H
#include "bsp_clk.h"
#include "bsp_delay.h"
#include "bsp_led.h"
#include "bsp_uart.h"
#include "bsp_int.h"
#include "bsp_icm20608.h"
#include "stdio.h"

// freertos header
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"  // 信号量
#include "queue.h"   // 队列

// baseline header (for sensor_packet_t and calculate_checksum)
#include "baseline.h"

void freertos_test2_loop(void);
void sensor_task2(void *param);
void uart_task2(void *param);
void led_task2(void *param);
void sensor_timer_init(void);
void sensor_timer_start(void);


#endif //__FREERTOS_UARTSEND_H