#ifndef _BSP_UART_H_
#define _BSP_UART_H_

#include "../../imx6ul/imx6ul.h"
#include "../gpio/bsp_gpio.h"

#define UART_BASELINE_METRICS 1

void uart_Init();
void uart_io_Init();
void uart_disable(UART_Type *uart);
void uart_enable(UART_Type *uart);
void uart_softreset(UART_Type *uart);
void putc(unsigned char c);
void puts(char *str);
void raise(int sig_nr);
unsigned char getc();
// unblocking read
int uart_try_read_byte(uint8_t *out);

// blocking read
uint8_t uart_read_blocking();

void uart_read_seq_and_account(void);

// Metrics accessors
#if UART_BASELINE_METRICS
void uart_get_metrics(uint32_t *bytes, uint32_t *overruns, uint32_t *lost, uint32_t *max_burst);
uint32_t uart_get_idle_counter(void);
void uart_reset_metrics(void);
void uart_drain_nonblocking(void);
#endif



#endif  // _BSP_UART_H_